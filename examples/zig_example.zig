// Zig example for libcsp goroutines
// Compile with: zig build-exe examples/zig_example.zig -I. -L.libs -lcsp -lpthread -fno-stack-check

const std = @import("std");
const c = @cImport({
    @cInclude("csp.h");
});

fn zig_worker(arg: ?*anyopaque) callconv(.C) void {
    const ch = @ptrCast(*c.csp_gochan_t, arg);
    std.debug.print("[Zig Goroutine] Sending message...\n", .{});
    _ = c.csp_gochan_send(ch, @intToPtr(*anyopaque, 1234));
    std.debug.print("[Zig Goroutine] Done.\n", .{});
}

fn real_main(arg: ?*anyopaque) callconv(.C) void {
    _ = arg;
    std.debug.print("--- Zig libcsp Example ---\n", .{});

    const ch = c.csp_gochan_new(1);

    _ = c.csp_proc_create(0, zig_worker, ch);

    std.debug.print("[Zig Main] Waiting for message...\n", .{});
    const val = @ptrToInt(c.csp_gochan_recv(ch, null));
    std.debug.print("[Zig Main] Received value: {d}\n", .{val});

    std.debug.print("--- Zig Example Complete ---\n", .{});
    std.os.exit(0);
}

pub fn main() !void {
    // Set environment variable
    try std.os.setenv("LIBCSP_PRODUCTION", "1", true);

    _ = c.csp_proc_create(0, real_main, null);

    // Zig doesn't have a direct way to get _Thread_local without extern
    extern threadlocal var csp_this_core: ?*c.csp_core_t;

    _ = c.csp_core_run(csp_this_core);
}
