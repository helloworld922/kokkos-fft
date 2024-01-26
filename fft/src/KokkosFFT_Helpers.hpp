#ifndef KOKKOSFFT_HELPERS_HPP
#define KOKKOSFFT_HELPERS_HPP

#include <Kokkos_Core.hpp>
#include "KokkosFFT_default_types.hpp"
#include "KokkosFFT_utils.hpp"

namespace KokkosFFT {
namespace Impl {
template <typename ViewType, std::size_t DIM = 1>
auto _get_shift(const ViewType& inout, axis_type<DIM> _axes,
                int direction = 1) {
  static_assert(DIM > 0,
                "_get_shift: Rank of shift axes must be "
                "larger than or equal to 1.");

  // Convert the input axes to be in the range of [0, rank-1]
  std::vector<int> axes;
  for (std::size_t i = 0; i < DIM; i++) {
    int axis = KokkosFFT::Impl::convert_negative_axis(inout, _axes.at(i));
    axes.push_back(axis);
  }

  // Assert if the elements are overlapped
  constexpr int rank = ViewType::rank();
  assert(!KokkosFFT::Impl::has_duplicate_values(axes));
  assert(!KokkosFFT::Impl::is_out_of_range_value_included(axes, rank));

  axis_type<rank> shift = {0};
  for (int i = 0; i < DIM; i++) {
    int axis       = axes.at(i);
    shift.at(axis) = inout.extent(axis) / 2 * direction;
  }
  return shift;
}

template <typename ExecutionSpace, typename ViewType>
void _roll(const ExecutionSpace& exec_space, ViewType& inout,
           axis_type<1> shift, axis_type<1> axes) {
  static_assert(ViewType::rank() == 1, "_roll: Rank of View must be 1.");
  std::size_t n0 = inout.extent(0);

  ViewType tmp("tmp", n0);
  std::size_t len = (n0 - 1) / 2 + 1;

  auto [_shift0, _shift1, _shift2] =
      KokkosFFT::Impl::convert_negative_shift(inout, shift.at(0), 0);
  int shift0 = _shift0, shift1 = _shift1, shift2 = _shift2;

  // shift2 == 0 means shift
  if (shift2 == 0) {
    Kokkos::parallel_for(
        Kokkos::RangePolicy<ExecutionSpace, Kokkos::IndexType<std::size_t>>(
            exec_space, 0, len),
        KOKKOS_LAMBDA(const int& i) {
          tmp(i + shift0) = inout(i);
          if (i + shift1 < n0) {
            tmp(i) = inout(i + shift1);
          }
        });

    inout = tmp;
  }
}

template <typename ExecutionSpace, typename ViewType, std::size_t DIM1 = 1>
void _roll(const ExecutionSpace& exec_space, ViewType& inout,
           axis_type<2> shift, axis_type<DIM1> axes) {
  constexpr std::size_t DIM0 = 2;
  static_assert(ViewType::rank() == DIM0, "_roll: Rank of View must be 2.");
  int n0 = inout.extent(0), n1 = inout.extent(1);

  ViewType tmp("tmp", n0, n1);
  int len0 = (n0 - 1) / 2 + 1;
  int len1 = (n1 - 1) / 2 + 1;

  using range_type = Kokkos::MDRangePolicy<
      ExecutionSpace,
      Kokkos::Rank<2, Kokkos::Iterate::Default, Kokkos::Iterate::Default>>;
  using tile_type  = typename range_type::tile_type;
  using point_type = typename range_type::point_type;

  range_type range(
      point_type{{0, 0}}, point_type{{len0, len1}}, tile_type{{4, 4}}
      // [TO DO] Choose optimal tile sizes for each device
  );

  axis_type<2> shift0 = {0}, shift1 = {0}, shift2 = {n0 / 2, n1 / 2};
  for (int i = 0; i < DIM1; i++) {
    int axis = axes.at(i);

    auto [_shift0, _shift1, _shift2] =
        KokkosFFT::Impl::convert_negative_shift(inout, shift.at(axis), axis);
    shift0.at(axis) = _shift0;
    shift1.at(axis) = _shift1;
    shift2.at(axis) = _shift2;
  }

  int shift_00 = shift0.at(0), shift_10 = shift0.at(1);
  int shift_01 = shift1.at(0), shift_11 = shift1.at(1);
  int shift_02 = shift2.at(0), shift_12 = shift2.at(1);

  Kokkos::parallel_for(
      range, KOKKOS_LAMBDA(int i0, int i1) {
        if (i0 + shift_00 < n0 && i1 + shift_10 < n1) {
          tmp(i0 + shift_00, i1 + shift_10) = inout(i0, i1);
        }

        if (i0 + shift_01 < n0 && i1 + shift_11 < n1) {
          tmp(i0, i1) = inout(i0 + shift_01, i1 + shift_11);
        }

        if (i0 + shift_01 < n0 && i1 + shift_10 < n1) {
          tmp(i0 + shift_02, i1 + shift_10 + shift_12) =
              inout(i0 + shift_01 + shift_02, i1 + shift_12);
        }

        if (i0 + shift_00 < n0 && i1 + shift_11 < n1) {
          tmp(i0 + shift_00 + shift_02, i1 + shift_12) =
              inout(i0 + shift_02, i1 + shift_11 + shift_12);
        }
      });

  inout = tmp;
}

template <typename ExecutionSpace, typename ViewType, std::size_t DIM = 1>
void _fftshift(const ExecutionSpace& exec_space, ViewType& inout,
               axis_type<DIM> axes) {
  static_assert(Kokkos::is_view<ViewType>::value,
                "_fftshift: ViewType is not a Kokkos::View.");
  static_assert(
      KokkosFFT::Impl::is_layout_left_or_right_v<ViewType>,
      "_fftshift: ViewType must be either LayoutLeft or LayoutRight.");
  static_assert(
      Kokkos::SpaceAccessibility<ExecutionSpace,
                                 typename ViewType::memory_space>::accessible,
      "_fftshift: execution_space cannot access data in ViewType");

  static_assert(ViewType::rank() >= DIM,
                "_fftshift: Rank of View must be larger thane "
                "or equal to the Rank of shift axes.");
  auto shift = _get_shift(inout, axes);
  _roll(exec_space, inout, shift, axes);
}

template <typename ExecutionSpace, typename ViewType, std::size_t DIM = 1>
void _ifftshift(const ExecutionSpace& exec_space, ViewType& inout,
                axis_type<DIM> axes) {
  static_assert(Kokkos::is_view<ViewType>::value,
                "_ifftshift: ViewType is not a Kokkos::View.");
  static_assert(
      KokkosFFT::Impl::is_layout_left_or_right_v<ViewType>,
      "_ifftshift: ViewType must be either LayoutLeft or LayoutRight.");
  static_assert(
      Kokkos::SpaceAccessibility<ExecutionSpace,
                                 typename ViewType::memory_space>::accessible,
      "_ifftshift: execution_space cannot access data in ViewType");

  static_assert(ViewType::rank() >= DIM,
                "_ifftshift: Rank of View must be larger "
                "thane or equal to the Rank of shift axes.");
  auto shift = _get_shift(inout, axes, -1);
  _roll(exec_space, inout, shift, axes);
}
}  // namespace Impl
}  // namespace KokkosFFT

namespace KokkosFFT {
template <typename ExecutionSpace, typename RealType>
auto fftfreq(const ExecutionSpace& exec_space, const std::size_t n,
             const RealType d = 1.0) {
  static_assert(std::is_floating_point<RealType>::value,
                "fftfreq: d must be float or double");
  using ViewType = Kokkos::View<RealType*, ExecutionSpace>;
  ViewType freq("freq", n);

  RealType val = 1.0 / (static_cast<RealType>(n) * d);
  int N1       = (n - 1) / 2 + 1;
  int N2       = n / 2;

  auto h_freq = Kokkos::create_mirror_view(freq);

  auto p1 = KokkosFFT::Impl::arange(0, N1);
  auto p2 = KokkosFFT::Impl::arange(-N2, 0);

  for (int i = 0; i < N1; i++) {
    h_freq(i) = static_cast<RealType>(p1.at(i)) * val;
  }
  for (int i = 0; i < N2; i++) {
    h_freq(i + N1) = static_cast<RealType>(p2.at(i)) * val;
  }
  Kokkos::deep_copy(freq, h_freq);

  return freq;
}

template <typename ExecutionSpace, typename RealType>
auto rfftfreq(const ExecutionSpace& exec_space, const std::size_t n,
              const RealType d = 1.0) {
  static_assert(std::is_floating_point<RealType>::value,
                "fftfreq: d must be float or double");
  using ViewType = Kokkos::View<RealType*, ExecutionSpace>;

  RealType val = 1.0 / (static_cast<RealType>(n) * d);
  int N        = n / 2 + 1;
  ViewType freq("freq", N);

  auto h_freq = Kokkos::create_mirror_view(freq);
  auto p      = KokkosFFT::Impl::arange(0, N);

  for (int i = 0; i < N; i++) {
    h_freq(i) = static_cast<RealType>(p.at(i)) * val;
  }
  Kokkos::deep_copy(freq, h_freq);

  return freq;
}

template <typename ExecutionSpace, typename ViewType>
void fftshift(const ExecutionSpace& exec_space, ViewType& inout) {
  constexpr std::size_t rank = ViewType::rank();
  constexpr int start        = -static_cast<int>(rank);
  axis_type<rank> axes       = KokkosFFT::Impl::index_sequence<rank>(start);
  KokkosFFT::Impl::_fftshift(exec_space, inout, axes);
}

template <typename ExecutionSpace, typename ViewType>
void fftshift(const ExecutionSpace& exec_space, ViewType& inout, int axes) {
  KokkosFFT::Impl::_fftshift(exec_space, inout, axis_type<1>{axes});
}

template <typename ExecutionSpace, typename ViewType, std::size_t DIM = 1>
void fftshift(const ExecutionSpace& exec_space, ViewType& inout,
              axis_type<DIM> axes) {
  KokkosFFT::Impl::_fftshift(exec_space, inout, axes);
}

template <typename ExecutionSpace, typename ViewType>
void ifftshift(const ExecutionSpace& exec_space, ViewType& inout) {
  constexpr std::size_t rank = ViewType::rank();
  constexpr int start        = -static_cast<int>(rank);
  axis_type<rank> axes       = KokkosFFT::Impl::index_sequence<rank>(start);
  KokkosFFT::Impl::_ifftshift(exec_space, inout, axes);
}

template <typename ExecutionSpace, typename ViewType>
void ifftshift(const ExecutionSpace& exec_space, ViewType& inout, int axes) {
  KokkosFFT::Impl::_ifftshift(exec_space, inout, axis_type<1>{axes});
}

template <typename ExecutionSpace, typename ViewType, std::size_t DIM = 1>
void ifftshift(const ExecutionSpace& exec_space, ViewType& inout,
               axis_type<DIM> axes) {
  KokkosFFT::Impl::_ifftshift(exec_space, inout, axes);
}
}  // namespace KokkosFFT

#endif