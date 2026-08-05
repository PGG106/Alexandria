#!/usr/bin/env python3

import argparse
import heapq
from pathlib import Path

import numpy as np


LANES = 768
GROUP_SIZE = 4
MASK_BYTES = LANES // 8
POPCOUNT = np.array([int(value).bit_count() for value in range(256)], dtype=np.uint8)


def load_masks(path: Path, sample_count: int) -> tuple[np.ndarray, int]:
    packed = np.memmap(path, dtype=np.uint8, mode="r")
    if packed.size == 0 or packed.size % MASK_BYTES != 0:
        raise ValueError(f"{path} does not contain complete {MASK_BYTES}-byte masks")
    packed = packed.reshape((-1, MASK_BYTES))
    source_count = packed.shape[0]
    if not 0 < sample_count <= source_count:
        raise ValueError(f"--samples must be between 1 and {source_count}")
    if sample_count < source_count:
        indices = np.linspace(0, source_count - 1, sample_count, dtype=np.int64)
        packed = packed[indices]
    active_masks = np.unpackbits(packed, axis=1, bitorder="little").astype(np.bool_)
    np.random.default_rng(0).shuffle(active_masks, axis=0)
    return active_masks, source_count


def marginal_permutation(zero_masks: np.ndarray) -> np.ndarray:
    activation_counts = (~zero_masks).sum(axis=0)
    return np.argsort(-activation_counts, kind="stable")


def load_marginal_permutation(path: Path) -> np.ndarray:
    values = np.fromstring(path.read_text(), dtype=np.uint64, sep=" ")
    if values.size != LANES + 1 or values[0] == 0 or np.any(values[1:] > values[0]):
        raise ValueError(f"{path} must contain a sample count and {LANES} activation counts")
    return np.argsort(-values[1:].astype(np.int64), kind="stable")


def zero_group_count(zero_masks: np.ndarray, permutation: np.ndarray) -> int:
    arranged = zero_masks[:, permutation].reshape((-1, LANES // GROUP_SIZE, GROUP_SIZE))
    return int(np.all(arranged, axis=2).sum())


def active_groups_per_sample(zero_masks: np.ndarray, permutation: np.ndarray) -> float:
    groups = zero_masks.shape[0] * LANES // GROUP_SIZE
    return (groups - zero_group_count(zero_masks, permutation)) / zero_masks.shape[0]


def score_changes(zero_masks: np.ndarray, permutation: np.ndarray) -> np.ndarray:
    arranged = zero_masks[:, permutation]
    blocks = arranged.reshape((-1, LANES // GROUP_SIZE, GROUP_SIZE))
    rest_zero = np.empty_like(blocks)
    for lane in range(GROUP_SIZE):
        rest_zero[:, :, lane] = np.all(np.delete(blocks, lane, axis=2), axis=2)
    rest_zero = rest_zero.reshape((-1, LANES))

    zero_packed = np.packbits(arranged.T, axis=1, bitorder="little")
    rest_packed = np.packbits(rest_zero.T, axis=1, bitorder="little")
    positive = np.empty((LANES, LANES), dtype=np.int32)
    for destination in range(LANES):
        intersections = np.bitwise_and(zero_packed, rest_packed[destination])
        positive[:, destination] = POPCOUNT[intersections].sum(axis=1, dtype=np.int32)

    changes = positive - np.diag(positive)[None, :]
    block_ids = np.arange(LANES) // GROUP_SIZE
    changes[block_ids[:, None] == block_ids[None, :]] = 0
    return changes


def apply_independent_swaps(changes: np.ndarray, permutation: np.ndarray) -> tuple[int, int]:
    gains = changes + changes.T
    swaps = 0
    total_gain = 0

    while True:
        flat_index = int(np.argmax(gains))
        gain = int(gains.flat[flat_index])
        if gain <= 0:
            break

        first, second = np.unravel_index(flat_index, gains.shape)
        permutation[first], permutation[second] = permutation[second], permutation[first]
        swaps += 1
        total_gain += gain

        for block in (first // GROUP_SIZE, second // GROUP_SIZE):
            start = block * GROUP_SIZE
            gains[start : start + GROUP_SIZE, :] = 0
            gains[:, start : start + GROUP_SIZE] = 0

    return swaps, total_gain


def best_cycle_for_pair(
    changes: np.ndarray, first_block: int, second_block: int, unavailable: np.ndarray
) -> tuple[int, int, int, int] | None:
    if unavailable[first_block] or unavailable[second_block] or first_block == second_block:
        return None

    first = np.arange(first_block * GROUP_SIZE, (first_block + 1) * GROUP_SIZE)
    second = np.arange(second_block * GROUP_SIZE, (second_block + 1) * GROUP_SIZE)
    available_lanes = np.repeat(~unavailable, GROUP_SIZE)
    available_lanes[first] = False
    available_lanes[second] = False
    third = np.flatnonzero(available_lanes)
    if third.size == 0:
        return None

    gains = (
        changes[np.ix_(first, second)][:, :, None]
        + changes[np.ix_(second, third)][None, :, :]
        + changes[np.ix_(third, first)].T[:, None, :]
    )
    flat_index = int(np.argmax(gains))
    gain = int(gains.flat[flat_index])
    if gain <= 0:
        return None

    first_lane, second_lane, third_lane = np.unravel_index(flat_index, gains.shape)
    return gain, int(first[first_lane]), int(second[second_lane]), int(third[third_lane])


def apply_independent_cycles(changes: np.ndarray, permutation: np.ndarray) -> tuple[int, int]:
    block_count = LANES // GROUP_SIZE
    unavailable = np.zeros(block_count, dtype=np.bool_)
    heap: list[tuple[int, int, int, int, int, int]] = []

    for first_block in range(block_count):
        for second_block in range(block_count):
            candidate = best_cycle_for_pair(changes, first_block, second_block, unavailable)
            if candidate is not None:
                gain, first, second, third = candidate
                heapq.heappush(heap, (-gain, first_block, second_block, first, second, third))

    cycles = 0
    total_gain = 0
    while heap:
        negative_gain, first_block, second_block, first, second, third = heapq.heappop(heap)
        if unavailable[first_block] or unavailable[second_block]:
            continue
        if unavailable[third // GROUP_SIZE]:
            candidate = best_cycle_for_pair(changes, first_block, second_block, unavailable)
            if candidate is not None:
                gain, first, second, third = candidate
                heapq.heappush(heap, (-gain, first_block, second_block, first, second, third))
            continue

        values = permutation[[first, second, third]].copy()
        permutation[first], permutation[second], permutation[third] = values[2], values[0], values[1]
        unavailable[[first_block, second_block, third // GROUP_SIZE]] = True
        cycles += 1
        total_gain -= negative_gain

    return cycles, total_gain


def optimize(zero_masks: np.ndarray, permutation: np.ndarray) -> np.ndarray:
    permutation = permutation.copy()
    for iteration in range(1, 51):
        before = zero_group_count(zero_masks, permutation)
        changes = score_changes(zero_masks, permutation)
        swaps, swap_gain = apply_independent_swaps(changes, permutation)
        after_swaps = zero_group_count(zero_masks, permutation)
        if after_swaps - before != swap_gain:
            raise RuntimeError("swap gain did not match the measured objective")

        changes = score_changes(zero_masks, permutation)
        cycles, cycle_gain = apply_independent_cycles(changes, permutation)
        after_cycles = zero_group_count(zero_masks, permutation)
        if after_cycles - after_swaps != cycle_gain:
            raise RuntimeError("cycle gain did not match the measured objective")

        print(
            f"iteration {iteration}: {swaps} swaps (+{swap_gain}), "
            f"{cycles} cycles (+{cycle_gain})"
        )
        if swap_gain == 0 and cycle_gain == 0:
            break
    else:
        raise RuntimeError("optimizer did not converge after 50 iterations")

    if sorted(permutation.tolist()) != list(range(LANES)):
        raise RuntimeError("optimizer produced a non-bijective permutation")
    return permutation


def main() -> None:
    parser = argparse.ArgumentParser(description="Optimize NNUE four-lane sparse groups")
    parser.add_argument("masks", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--train", type=int, default=10_000)
    parser.add_argument("--samples", type=int, default=100_000)
    parser.add_argument("--initial-profile", type=Path)
    args = parser.parse_args()

    active_masks, source_count = load_masks(args.masks, args.samples)
    if not 0 < args.train < active_masks.shape[0]:
        raise ValueError("--train must leave at least one held-out mask")
    zero_masks = ~active_masks
    training = zero_masks[: args.train]
    held_out = zero_masks[args.train :]
    baseline = (
        load_marginal_permutation(args.initial_profile)
        if args.initial_profile is not None
        else marginal_permutation(training)
    )

    print(
        f"sampled {active_masks.shape[0]} of {source_count} masks; "
        f"training={args.train}, held-out={held_out.shape[0]}"
    )
    print(f"baseline training active groups: {active_groups_per_sample(training, baseline):.6f}")
    print(f"baseline held-out active groups: {active_groups_per_sample(held_out, baseline):.6f}")
    optimized = optimize(training, baseline)
    print(f"optimized training active groups: {active_groups_per_sample(training, optimized):.6f}")
    print(f"optimized held-out active groups: {active_groups_per_sample(held_out, optimized):.6f}")

    with args.output.open("w") as output:
        print(args.train, file=output)
        for lane in optimized:
            print(int(lane), file=output)


if __name__ == "__main__":
    main()