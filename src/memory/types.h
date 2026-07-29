#pragma once

#include <memory>

using i8  = signed char;
using i16 = signed short;
using i32 = signed int;
using i64 = signed long long;

using u8    = unsigned char;
using u16   = unsigned short;
using u32   = unsigned int;
using u64   = unsigned long long;
using usize = unsigned long;

using f32 = float;
using f64 = double;

#define KiB(size) (((u64)size) << 10)
#define MiB(size) (((u64)size) << 20)
#define GiB(size) (((u64)size) << 30)

template <typename T>
using UP = std::unique_ptr<T>;

template <typename T>
using SP = std::shared_ptr<T>;
