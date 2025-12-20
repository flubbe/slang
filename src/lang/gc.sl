/**
 * Garbage collector interface.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

/** Run the garbage collector */
#[native(lib="gc")]
fn run() -> void;

/** Get the object count. */
#[native(lib="gc")]
fn object_count() -> i32;

/** Get the root set size. */
#[native(lib="gc")]
fn root_set_size() -> i32;

/** Get allocated bytes. */
#[native(lib="gc")]
fn allocated_bytes() -> i32;

/** Get allocated bytes since last GC run. */
#[native(lib="gc")]
fn allocated_bytes_since_gc() -> i32;

/** Minimal allocated bytes that trigger GC run. */
#[native(lib="gc")]
fn min_threshold_bytes() -> i32;

/** Threshold when to trigger GC run. */
#[native(lib="gc")]
fn threshold_bytes() -> i32;

/** Growth factor for the live set that triggers a GC run. */
#[native(lib="gc")]
fn growth_factor() -> f32;
