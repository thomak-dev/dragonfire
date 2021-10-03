#pragma once

// primary template handles types that have no nested ::CType member:
template <typename, typename = void>
struct has_ctype_member : std::false_type
{
};

// specialization recognizes types that do have a nested ::CType member:
template <typename T>
struct has_ctype_member<T, std::void_t<typename T::CType>> : std::true_type
{
};