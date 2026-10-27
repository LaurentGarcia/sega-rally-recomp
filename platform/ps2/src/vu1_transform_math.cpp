#include "vu1_transform.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>

namespace ps2 {
namespace {
bool valid(float v) {
    const auto b=std::bit_cast<std::uint32_t>(v)&0x7fffffffu;
    return b==0 || (b>=0x00800000u && b<=0x47800000u);
}
std::array<float,4> lanes(const TransformVector4&v) { return std::bit_cast<std::array<float,4>>(v); }
bool domain(const TransformMatrix&a,const TransformMatrix&b,std::span<const TransformVector4>v) {
    if(v.size()>vu1_transform_capacity || (!v.empty()&&!v.data()))return false;
    for(float f:a)if(!valid(f))return false;
    for(float f:b)if(!valid(f))return false;
    if(a[3]!=0 || a[7]!=0 || a[11]!=0 || a[15]!=1)return false;
    for(const auto&p:v){for(float f:lanes(p))if(!valid(f))return false;if(p.w!=1)return false;}
    return true;
}
// Upper bound for a four-term dot product. A single +/-1 coefficient copies
// a normal input exactly (zero terms and signed-zero differences do not affect
// absolute error). Other rows count every multiply and every addition, even
// zero terms. u=2^-22 is two binary32 ulps relative to a normal magnitude.
double row_error(const TransformMatrix&m,unsigned row,double magnitude) {
    unsigned nonzero=0;float coefficient=0;
    for(unsigned c=0;c<4;++c)if(m[c*4+row]!=0){++nonzero;coefficient=m[c*4+row];}
    if(!nonzero || (nonzero==1 && (coefficient==1 || coefficient==-1)))return 0;
    constexpr double u=0x1p-22, underflow=0x1p-126;
    constexpr double gamma=(7*u)/(1-7*u);
    return gamma*magnitude + 8*underflow;
}
}
bool bound_vu1_transform(const TransformMatrix&a,const TransformMatrix&b,
                        std::span<const TransformVector4>v,TransformErrorBound&out) noexcept {
    if(!domain(a,b,v))return false;
    std::array<double,4> maximum{},world{},world_error{};
    for(const auto&p:v){const auto f=lanes(p);for(unsigned c=0;c<4;++c)maximum[c]=std::max(maximum[c],std::abs(double(f[c])));}
    for(unsigned r=0;r<4;++r){
        for(unsigned c=0;c<4;++c)world[r]+=std::abs(double(a[c*4+r]))*maximum[c];
        world_error[r]=row_error(a,r,world[r]);
        if(world[r]+world_error[r]>65536)return false;
    }
    TransformErrorBound result;
    for(unsigned r=0;r<4;++r){
        double magnitude=0, propagated=0;
        for(unsigned c=0;c<4;++c){const double coefficient=std::abs(double(b[c*4+r]));
            magnitude+=coefficient*(world[c]+world_error[c]);propagated+=coefficient*world_error[c];}
        // Inflate for the binary64 bound calculation and reference dot-product
        // rounding. This does not assume host and EE float operation agreement.
        result.absolute[r]=(propagated+row_error(b,r,magnitude)+0x1p-24*magnitude+
                            0x1p-48*(magnitude+1))* (1+0x1p-40);
        if(magnitude+result.absolute[r]>65536)return false;
    }
    out=result;return true;
}
TransformGuard guard_vu1_projection(const TransformVector4&v,const TransformErrorBound&e,
                                   const TransformProjectionBudget&budget,TransformProjectionError*out) noexcept {
    const auto p=lanes(v);
    for(float f:p)if(!valid(f))return TransformGuard::invalid;
    for(double x:e.absolute)if(!std::isfinite(x)||x<0)return TransformGuard::invalid;
    if(!std::isfinite(budget.pixels)||!std::isfinite(budget.depth)||budget.pixels<0||budget.depth<0)return TransformGuard::invalid;
    const auto&d=e.absolute;const double x=v.x,y=v.y,z=v.z,w=v.w;
    if(w-d[3]<=0x1p-10 || w+x<=d[3]+d[0] || w-x<=d[3]+d[0] ||
       w+y<=d[3]+d[1] || w-y<=d[3]+d[1] || z<=d[2] || w-z<=d[3]+d[2])
        return TransformGuard::near_clip_plane;
    const double denominator=w*(w-d[3]);
    const auto quotient_error=[&](double coordinate,double error){return (error*w+std::abs(coordinate)*d[3])/denominator;};
    TransformProjectionError estimate{std::max(248*quotient_error(x,d[0]),192*quotient_error(y,d[1])),quotient_error(z,d[2])};
    if(out)*out=estimate;
    return estimate.pixels<=budget.pixels && estimate.depth<=budget.depth ? TransformGuard::accepted:TransformGuard::projection_budget;
}
bool prepare_vu1_projection_guard(const TransformErrorBound&e,const TransformProjectionBudget&budget,PreparedProjectionGuard&out) noexcept {
    for(double x:e.absolute)if(!std::isfinite(x)||x<0)return false;
    if(!std::isfinite(budget.pixels)||!std::isfinite(budget.depth)||budget.pixels<0||budget.depth<0)return false;
    PreparedProjectionGuard p;p.error_=e;p.budget_=budget;p.valid_=true;p.unit_w_=true;
    const auto&d=e.absolute;
    // Normal-or-zero binary32 thresholds; conversions are corrected by exact
    // binary64 comparisons, independent of the EE's narrowing direction.
    const auto floor_bits=[](double x,bool strict){
        if(x<=0)return std::uint32_t(0);
        auto bits=std::bit_cast<std::uint32_t>(float(std::min(x,65536.0)));
        const double rounded=std::bit_cast<float>(bits);
        if(rounded>x || (strict&&rounded==x))--bits;
        return bits<0x00800000u?std::uint32_t(0):bits;
    };
    const auto above_bits=[](double x){
        auto bits=std::bit_cast<std::uint32_t>(float(x));
        if(double(std::bit_cast<float>(bits))<=x)++bits;
        return std::max(bits,std::uint32_t(0x00800000u));
    };
    const std::array<double,3> margin{1-(d[3]+d[0]),1-(d[3]+d[1]),1-(d[3]+d[2])};
    if(1-d[3]<=0x1p-10 || margin[0]<=0 || margin[1]<=0 || margin[2]<=d[2]){out=p;return true;}
    p.plane_={floor_bits(margin[0],true),floor_bits(margin[1],true),above_bits(d[2]),floor_bits(margin[2],true)};
    // Verify strict-plane endpoints in the original expression order. Large
    // cancellation in threshold construction may otherwise round outward.
    for(unsigned axis=0;axis<3;++axis){const unsigned index=axis==2?3:axis;
        unsigned attempts=0;
        while(1-double(std::bit_cast<float>(p.plane_[index]))<=d[3]+d[axis]){
            if(!p.plane_[index] || ++attempts>4){p.unit_w_=false;out=p;return true;}--p.plane_[index];}}
    if(p.plane_[2]>p.plane_[3]){out=p;return true;}
    p.plane_possible_=true;
    const auto allowed=[&](double error,double budget_value){return d[3]==0?(error<=budget_value?1.0:-1.0):(budget_value*(1-d[3])-error)/d[3];};
    const std::array<double,3>threshold{allowed(d[0],budget.pixels/248),allowed(d[1],budget.pixels/192),allowed(d[2],budget.depth)};
    if(threshold[0]<0||threshold[1]<0||threshold[2]<0){out=p;return true;}
    for(unsigned axis=0;axis<3;++axis)p.budget_limit_[axis]=floor_bits(std::min(threshold[axis],1.0),false);
    const auto estimate=[&](unsigned axis,std::uint32_t bits){return (d[axis]+double(std::bit_cast<float>(bits))*d[3])/(1-d[3]);};
    const std::array<double,3>budgets{budget.pixels,budget.pixels,budget.depth};
    const std::array<double,3>scales{248,192,1};
    for(unsigned axis=0;axis<3;++axis){
        unsigned attempts=0;
        while(p.budget_limit_[axis] && scales[axis]*estimate(axis,p.budget_limit_[axis])>budgets[axis]){
            if(++attempts>4){p.unit_w_=false;out=p;return true;}--p.budget_limit_[axis];}
        if(scales[axis]*estimate(axis,p.budget_limit_[axis])>budgets[axis]){out=p;return true;}
    }
    if(p.budget_limit_[2]<p.plane_[2]){out=p;return true;}
    p.budget_possible_=true;
    p.estimate_={std::max(248*estimate(0,std::min(p.plane_[0],p.budget_limit_[0])),192*estimate(1,std::min(p.plane_[1],p.budget_limit_[1]))),
        estimate(2,std::min(p.plane_[3],p.budget_limit_[2]))};
    out=p;return true;
}
TransformGuard guard_prepared_vu1_projection(const TransformVector4&v,const PreparedProjectionGuard&p,TransformProjectionError*out) noexcept {
    if(!p.valid_)return TransformGuard::invalid;
    const auto raw=std::bit_cast<std::array<std::uint32_t,4>>(v);
    if(!p.unit_w_||raw[3]!=0x3f800000u)return guard_vu1_projection(v,p.error_,p.budget_,out);
    std::array<std::uint32_t,3>magnitude;
    for(unsigned i=0;i<3;++i){magnitude[i]=raw[i]&0x7fffffffu;
        if(magnitude[i] && (magnitude[i]<0x00800000u||magnitude[i]>0x47800000u))return TransformGuard::invalid;}
    if(!p.plane_possible_||magnitude[0]>p.plane_[0]||magnitude[1]>p.plane_[1]||raw[2]<p.plane_[2]||raw[2]>p.plane_[3])return TransformGuard::near_clip_plane;
    if(!p.budget_possible_||magnitude[0]>p.budget_limit_[0]||magnitude[1]>p.budget_limit_[1]||raw[2]>p.budget_limit_[2])return TransformGuard::projection_budget;
    if(out)*out=p.estimate_;return TransformGuard::accepted;
}
bool transform_reference(const TransformMatrix&a,const TransformMatrix&b,
                         std::span<const TransformVector4>input,std::span<TransformVector4>output) noexcept {
    if(!domain(a,b,input)||output.size()<input.size()||(!input.empty()&&!output.data()))return false;
    std::array<TransformVector4,vu1_transform_capacity> scratch;
    for(std::size_t i=0;i<input.size();++i){
        const auto p=lanes(input[i]);std::array<double,4>world{},clip{};
        for(unsigned r=0;r<4;++r){for(unsigned c=0;c<4;++c)world[r]+=double(a[c*4+r])*p[c];
            if(!std::isfinite(world[r])||std::abs(world[r])>65536)return false;}
        std::array<float,4> result;
        for(unsigned r=0;r<4;++r){for(unsigned c=0;c<4;++c)clip[r]+=double(b[c*4+r])*world[c];
            if(!std::isfinite(clip[r])||std::abs(clip[r])>65536)return false;
            result[r]=float(clip[r]);if(!valid(result[r]))return false;}
        scratch[i]=std::bit_cast<TransformVector4>(result);
    }
    std::copy_n(scratch.begin(),input.size(),output.begin());return true;
}
} // namespace ps2
