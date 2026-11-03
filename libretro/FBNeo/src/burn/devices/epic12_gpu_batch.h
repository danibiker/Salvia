// Shared Xenos submission planner and atlas geometry. Overlapping draws retain order.
#ifndef FBNEO_EPIC12_GPU_BATCH_H
#define FBNEO_EPIC12_GPU_BATCH_H

static int epic12_gpu_dest_mode(const Epic12GpuCommand &c)
{
	return (c.dmode == 4 || c.dmode == 7 || c.dmode == 3) ? 0 : c.dmode;
}

static int epic12_gpu_dest_alpha(const Epic12GpuCommand &c)
{
	if (c.dmode == 7 || c.dmode == 3) return 31;
	return c.dmode == 4 ? 31 - c.da : c.da;
}

static bool epic12_gpu_additive(const Epic12GpuCommand &c)
{
	return c.blend && epic12_gpu_dest_mode(c) == 0 && epic12_gpu_dest_alpha(c) == 31;
}

static bool epic12_gpu_feedback(const Epic12GpuCommand &c)
{
#ifdef EPIC12_GPU_TEST
	return c.blend && (epic12_gpu_dest_mode(c) != 0 ||
		(epic12_gpu_dest_alpha(c) != 0 && epic12_gpu_dest_alpha(c) != 31));
#else
	return c.blend && epic12_gpu_dest_mode(c) != 0 &&
		!(epic12_gpu_dest_mode(c) == 1 && c.sa == 0);
#endif
}

static inline int epic12_gpu_blend_state(const Epic12GpuCommand &c)
{
	if (!c.blend) return 0;
#ifndef EPIC12_GPU_TEST
	if (epic12_gpu_dest_mode(c) == 1 && c.sa == 0) return 200;
#endif
	if (epic12_gpu_dest_mode(c) != 0) return 100;
	int da = epic12_gpu_dest_alpha(c);
	if (da == 0) return 0;
	if (da == 31) return 1;
	return 2 + da;
}

// Blend classification helper for reorder lookahead: 0 = plain, 1 = additive, 2..32 = hw factor, 100 = feedback.
static inline int epic12_gpu_command_blend_type(const Epic12GpuCommand &c)
{
	return epic12_gpu_blend_state(c);
}

// Source atlas contents are immutable for a batch. Two commands therefore
// commute when their destination rectangles are disjoint, including all RGB
// and alpha writes and destination-dependent blending. Pull the nearest safe
// matching draw type forward in an eight-command window; every overlapping
// pair retains its original order. Shader arithmetic and command fields do not
// change. Run after alpha cropping and before group/snapshot planning, and only
// on the attribute path. Separate input/output preserves the original list;
// input == output is supported for an already mutable cropped list.
static unsigned epic12_gpu_reorder(const Epic12GpuCommand *input, int count,
	Epic12GpuCommand *output)
{
	if (input != output) {
		memcpy(output, input, count * sizeof(Epic12GpuCommand));
	}
	unsigned moved = 0;
	for (int pos = 1; pos < count; ++pos) {
		const Epic12GpuCommand &previous = output[pos - 1];
		int want = epic12_gpu_command_blend_type(previous);
		const Epic12GpuCommand &current = output[pos];
		int currentType = epic12_gpu_command_blend_type(current);
		if (currentType == want) continue;
		int limit = pos + 8; if (limit > count) limit = count;
		for (int candidate = pos + 1; candidate < limit; ++candidate) {
			const Epic12GpuCommand &c = output[candidate];
			int type = epic12_gpu_command_blend_type(c);
			if (type != want) continue;
			rectangle dest(c.x, c.x + c.w - 1, c.y, c.y + c.h - 1);
			bool overlap = false;
			for (int i = pos; i < candidate; ++i) {
				const Epic12GpuCommand &p = output[i];
				if (epic12_gpu_intersects(p.x, p.y, p.w, p.h, dest)) { overlap = true; break; }
			}
			if (overlap) continue;
			Epic12GpuCommand saved = c;
			memmove(&output[pos + 1], &output[pos], (candidate - pos) * sizeof(Epic12GpuCommand));
			output[pos] = saved;
			++moved;
			break;
		}
	}
	return moved;
}

static bool epic12_gpu_same_state(const Epic12GpuCommand &a, const Epic12GpuCommand &b)
{
	if (a.transparent != b.transparent || a.blend != b.blend ||
		a.tint.r != b.tint.r || a.tint.g != b.tint.g || a.tint.b != b.tint.b) return false;
	if (!a.blend) return true;
	if (a.sa != b.sa || epic12_gpu_dest_mode(a) != epic12_gpu_dest_mode(b)) return false;
	return epic12_gpu_dest_mode(a) == 1 || epic12_gpu_dest_alpha(a) == epic12_gpu_dest_alpha(b);
}

static rectangle epic12_gpu_resolve_rect(const Epic12GpuCommand &c, const rectangle &bounds)
{
	int x = c.x - bounds.min_x, y = c.y - bounds.min_y;
	return rectangle(x & ~7, ((x + c.w + 7) & ~7) - 1,
		y & ~7, ((y + c.h + 7) & ~7) - 1);
}

static unsigned epic12_gpu_rect_area(const rectangle &r)
{
	return (r.max_x - r.min_x + 1) * (r.max_y - r.min_y + 1);
}

// Feedback draws can share a snapshot only if their destinations are disjoint.
// Limit lookahead and the copied area so sparse sprites don't turn a small
// local resolve into repeated full-screen transfers.
static int epic12_gpu_group_end(const Epic12GpuCommand *cmd, int first, int count,
	const rectangle &bounds, rectangle &snapshot, bool attributes = true)
{
	const Epic12GpuCommand &head = cmd[first];
	bool feedback = epic12_gpu_feedback(head);
	int headBlend = attributes ? epic12_gpu_blend_state(head) : 0;
	snapshot = epic12_gpu_resolve_rect(head, bounds);
	unsigned area = epic12_gpu_rect_area(snapshot);
	int end = first + 1;
	for (; end < count; ++end) {
		const Epic12GpuCommand &c = cmd[end];
		if (attributes) {
			// Tint, alpha and transparency travel with each rectangle. Only
			// hardware blend state and destination-read dependencies split it.
			if (headBlend != epic12_gpu_blend_state(c)) break;
		} else if (!epic12_gpu_same_state(head, c)) break;
		if (!feedback) continue;
		if (end - first == 64) break;
		rectangle dest(c.x, c.x + c.w - 1, c.y, c.y + c.h - 1);
		bool overlap = false;
		for (int j = first; j < end; ++j) {
			const Epic12GpuCommand &p = cmd[j];
			if (epic12_gpu_intersects(p.x, p.y, p.w, p.h, dest)) { overlap = true; break; }
		}
		if (overlap) break;
		rectangle r = epic12_gpu_resolve_rect(c, bounds), merged = snapshot;
		if (r.min_x < merged.min_x) merged.min_x = r.min_x;
		if (r.min_y < merged.min_y) merged.min_y = r.min_y;
		if (r.max_x > merged.max_x) merged.max_x = r.max_x;
		if (r.max_y > merged.max_y) merged.max_y = r.max_y;
		unsigned nextArea = area + epic12_gpu_rect_area(r);
		if (epic12_gpu_rect_area(merged) > nextArea * 2) break;
		snapshot = merged; area = nextArea;
	}
	return end;
}

// Extend an existing feedback group's snapshot across later draw groups.
// Draw order and group boundaries stay unchanged. A later feedback group can
// read this earlier snapshot only if none of its destinations was written by
// ANY preceding command in the window, including plain and additive draws.
// Return the exclusive end of the last completely covered feedback group;
// the caller must still submit each original group and its own blend/shader.
static int epic12_gpu_snapshot_end(const Epic12GpuCommand *cmd, int first,
	int groupEnd, int count, const rectangle &bounds, rectangle &snapshot,
	bool attributes = true)
{
	if (!epic12_gpu_feedback(cmd[first])) return groupEnd;
	int limit = first + 32; if (limit > count) limit = count;
	int coveredEnd = groupEnd, scan = groupEnd;
	unsigned sumArea = epic12_gpu_rect_area(snapshot);
	while (scan < limit) {
		// Non-feedback draws do not read this texture, but their destinations
		// remain in the preceding-write range below. Limit this scan even if
		// one plain/additive draw group contains hundreds of commands.
		while (scan < limit && !epic12_gpu_feedback(cmd[scan])) ++scan;
		if (scan >= limit) break;
		rectangle next;
		int end = epic12_gpu_group_end(cmd, scan, count, bounds, next, attributes);
		// Never cover only part of a future group: the actual submitter will
		// use the complete original group without recomputing its boundary.
		if (end > limit) break;
		bool overlap = false;
		for (int i = scan; i < end && !overlap; ++i) {
			const Epic12GpuCommand &c = cmd[i];
			rectangle dest(c.x, c.x + c.w - 1, c.y, c.y + c.h - 1);
			for (int j = first; j < scan; ++j) {
				const Epic12GpuCommand &p = cmd[j];
				if (epic12_gpu_intersects(p.x, p.y, p.w, p.h, dest)) { overlap = true; break; }
			}
		}
		if (overlap) break;
		rectangle merged = snapshot;
		if (next.min_x < merged.min_x) merged.min_x = next.min_x;
		if (next.min_y < merged.min_y) merged.min_y = next.min_y;
		if (next.max_x > merged.max_x) merged.max_x = next.max_x;
		if (next.max_y > merged.max_y) merged.max_y = next.max_y;
		unsigned nextArea = sumArea + epic12_gpu_rect_area(next);
		if (epic12_gpu_rect_area(merged) > nextArea * 2) break;
		snapshot = merged; sumArea = nextArea; coveredEnd = scan = end;
	}
	return coveredEnd;
}

// Xenos UBYTE4 expands the low byte into x (including its endian conversion).
// Keep integers packed: eight per-vertex float conversions are costly on Xenon.
static UINT32 epic12_gpu_pack4(unsigned x, unsigned y, unsigned z, unsigned w)
{
	return x | (y << 8) | (z << 16) | (w << 24);
}

// USHORT2 uses two native 16-bit elements (Xenos 8IN16 endian conversion),
// whereas UBYTE4 above is a packed DWORD. Geometry remains entirely integer
// on the CPU; the vertex shader normalizes atlas coordinates on the GPU.
struct Epic12GpuVertex { UINT16 x, y, u, v; UINT32 tintAlpha, mode; };
static const int EPIC12_GPU_DRAW_RECTS = 256;

static int epic12_gpu_vertex_count(const Epic12GpuCommand &c)
{
	return (((c.sx + c.w - 1) >> 7) - (c.sx >> 7) + 1) *
		(((c.sy + c.h - 1) >> 7) - (c.sy >> 7) + 1) * 3;
}

// Xenos RECTLIST consumes TL, TR, BL; the fourth corner is implicit. Source
// page boundaries split the geometry without changing pixel centres or order.
static int epic12_gpu_vertices(const Epic12GpuCommand &c, const rectangle &bounds,
	const int *slots, Epic12GpuVertex *out)
{
	int n = 0;
	UINT32 tint = epic12_gpu_pack4(c.tint.r, c.tint.g, c.tint.b, c.sa);
	UINT32 mode = epic12_gpu_pack4(c.transparent, c.blend,
		epic12_gpu_dest_mode(c), epic12_gpu_dest_alpha(c));
	for (int sy = c.sy; sy < c.sy + c.h;) {
		int ey = ((sy >> 7) + 1) * 128; if (ey > c.sy + c.h) ey = c.sy + c.h;
		int slot_row = (sy >> 7) * 64;
		int dy = c.y + (c.flipy ? c.h - (ey - c.sy) : sy - c.sy) - bounds.min_y;
		int h_span = ey - sy;
		for (int sx = c.sx; sx < c.sx + c.w;) {
			int ex = ((sx >> 7) + 1) * 128; if (ex > c.sx + c.w) ex = c.sx + c.w;
			int slot = slots[slot_row + (sx >> 7)];
			int u0 = (slot & 15) * 128 + (sx & 127), u1 = u0 + ex - sx;
			int v0 = (slot >> 4) * 128 + (sy & 127), v1 = v0 + h_span;
			int dx = c.x + (c.flipx ? c.w - (ex - c.sx) : sx - c.sx) - bounds.min_x;
			int w_span = ex - sx;
			Epic12GpuVertex v[3] = {
				{(UINT16)dx, (UINT16)dy, (UINT16)(c.flipx ? u1 : u0), (UINT16)(c.flipy ? v1 : v0), tint, mode},
				{(UINT16)(dx + w_span), (UINT16)dy, (UINT16)(c.flipx ? u0 : u1), (UINT16)(c.flipy ? v1 : v0), tint, mode},
				{(UINT16)dx, (UINT16)(dy + h_span), (UINT16)(c.flipx ? u1 : u0), (UINT16)(c.flipy ? v0 : v1), tint, mode}};
			out[n++] = v[0]; out[n++] = v[1]; out[n++] = v[2];
			sx = ex;
		}
		sy = ey;
	}
	return n;
}
#endif
