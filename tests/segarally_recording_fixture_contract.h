// Synthetic fixture binding only; the actual-game wrapper emits immutable
// measured constants instead of this test-owned value.
#pragma once
namespace srally::recording_contract {
extern recording_reference::BuildBinding fixture;
inline recording_reference::BuildBinding binding() { return fixture; }
}
