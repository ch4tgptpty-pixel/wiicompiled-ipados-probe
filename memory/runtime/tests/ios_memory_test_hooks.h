#pragma once
#include <cstddef>
#ifndef MKW_IOS_MEMORY_TESTING
#error "Test hooks require MKW_IOS_MEMORY_TESTING"
#endif
namespace GuestFlat::Test {
enum class FailurePoint { Reserve, Protect, Backing, Remap };
void SetFailure(FailurePoint point, unsigned occurrence);
void ClearFailure();
size_t LiveAllocationCount();
}
