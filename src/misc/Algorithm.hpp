#pragma once

NS_HWM_BEGIN

//! 2つの範囲の要素同士を加算して、OutputIteratorに順番に書き込む。
/*! [first1, last1)と[first2, last2)の長さが異なる場合は、短い方の長さの分だけ処理する。
 */
template<class InputIterator1, class InputIterator2, class OutputIterator>
static constexpr
auto add(InputIterator1 first1, InputIterator1 last1, InputIterator2 first2, InputIterator2 last2, OutputIterator dest)
{
    auto len = std::min<UInt32>(last1 - first1, last2 - first2);
    return std::transform(first1, first1 + len, first2,
                          dest,
                          [](auto const &lhs, auto const &rhs) { return lhs + rhs; }
                          );
}

NS_HWM_END
