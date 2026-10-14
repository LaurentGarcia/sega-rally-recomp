#include "runtime/model2_priority.h"

#include <assert.h>
#include <stdio.h>

int main(void)
{
    const uint32_t one = 0x3f800000u;
    assert(daytona_model2_z_bucket(-1.0f, one) == 0);
    assert(daytona_model2_z_bucket(0.0f, one) == 0);
    assert(daytona_model2_z_bucket(1.0f, one) == 0x1000);
    assert(daytona_model2_z_bucket(0.5f, one) == 0x0800);
    assert(daytona_model2_z_bucket(2.0f, one) == 0x2000);
    assert(daytona_model2_z_bucket(32768.0f, one) == 0xffff);
    assert(daytona_model2_z_bucket(1.0f, 0) == 0xffff);
    // Window ownership beats camera distance; equal buckets choose the
    // latest polygon even if small geometry changes move its camera Z.
    assert(daytona_model2_order_compare(2, 0xffff, 1, 1, 0, 20) < 0);
    assert(daytona_model2_order_compare(1, 0x1000, 1, 1, 0x2000, 20) < 0);
    assert(daytona_model2_z_bucket(1.0f, one) == daytona_model2_z_bucket(1.00001f, one));
    assert(daytona_model2_order_compare(1, 0x1000, 20, 1, 0x1000, 1) < 0);
    assert(daytona_model2_order_compare(1, 0x1000, 20, 1, 0x1000, 20) == 0);
    assert(!daytona_model2_polygon_visible(0x100, 0));
    assert(daytona_model2_polygon_visible(0x100, 1));
    assert(daytona_model2_polygon_visible(0x20100, 0));
    assert(!daytona_model2_polygon_visible(0x20000, 1));
    float previous = 0;
    for (size_t rank = 0; rank < 4096; ++rank) {
        float depth = daytona_model2_rank_depth(rank, 4096);
        assert(depth > previous && depth < 1);
        previous = depth;
    }
    puts("Model 2 priority: C API, bucket, window, submission and depth contracts passed");
    return 0;
}
