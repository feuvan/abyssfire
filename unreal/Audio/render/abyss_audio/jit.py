"""Optional numba JIT for the per-sample kernels; falls back to plain Python (same results, ~100x slower)."""

try:  # pragma: no cover - depends on the environment
    from numba import njit as _njit

    def njit(fn):
        return _njit(cache=True, fastmath=False)(fn)

    HAVE_NUMBA = True
except ImportError:  # pragma: no cover
    def njit(fn):
        return fn

    HAVE_NUMBA = False
