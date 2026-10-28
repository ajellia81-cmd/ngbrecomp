// VF3 production interpolation, extracted unchanged; see README.md.
#include "r3d_scene.h"
#include <atomic>
#include <algorithm>
#include <tuple>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <limits>
#include <map>
namespace video {
Mat4 Mat4::identity()
{
	Mat4 r = {};
	r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1;
	return r;
}

Mat4 Mat4::operator*(const Mat4 &b) const
{
	Mat4 r;
	for (int i = 0; i < 4; i++)
		for (int j = 0; j < 4; j++)
			r.m[j * 4 + i] = m[i] * b.m[j * 4] + m[4 + i] * b.m[j * 4 + 1] + m[8 + i] * b.m[j * 4 + 2] + m[12 + i] * b.m[j * 4 + 3];
	return r;
}

namespace {

float moved2(const Model &p, const Model &m)
{
	float d = 0;
	for (int r = 0; r < 3; r++) {
		const float e = p.matrix.m[12 + r] - m.matrix.m[12 + r];
		d += e * e;
	}
	return d;
}

// An affine matrix's inverse (false if it has none)
bool affine_inverse(const Mat4 &a, Mat4 &r)
{
	const float *m = a.m;
	const float c00 = m[5] * m[10] - m[9] * m[6], c01 = m[9] * m[2] - m[1] * m[10], c02 = m[1] * m[6] - m[5] * m[2];
	const float det = m[0] * c00 + m[4] * c01 + m[8] * c02;
	if (!(std::fabs(det) > 1e-20f)) return false;
	const float k = 1.0f / det;
	r = Mat4::identity();
	r.m[0] = c00 * k; r.m[1] = c01 * k; r.m[2] = c02 * k;
	r.m[4] = (m[8] * m[6] - m[4] * m[10]) * k; r.m[5] = (m[0] * m[10] - m[8] * m[2]) * k; r.m[6] = (m[4] * m[2] - m[0] * m[6]) * k;
	r.m[8] = (m[4] * m[9] - m[8] * m[5]) * k; r.m[9] = (m[8] * m[1] - m[0] * m[9]) * k; r.m[10] = (m[0] * m[5] - m[4] * m[1]) * k;
	for (int i = 0; i < 3; i++) r.m[12 + i] = -(r.m[i] * m[12] + r.m[4 + i] * m[13] + r.m[8 + i] * m[14]);
	return true;
}

// Where a's model would be in b if it stood still while the camera moved by c (b = c a)
float predicted2(const Mat4 &c, const Model &p, const Model &m)
{
	float d = 0;
	for (int r = 0; r < 3; r++) {
		const float x = c.m[r] * p.matrix.m[12] + c.m[4 + r] * p.matrix.m[13] + c.m[8 + r] * p.matrix.m[14] + c.m[12 + r];
		const float e = x - m.matrix.m[12 + r];
		d += e * e;
	}
	return d;
}

// How differently two models point (their axes, a's turned by the camera's move c if known): 0 the same, up to 12
float turned2(const Mat4 *c, const Model &p, const Model &m)
{
	float d = 0;
	for (int j = 0; j < 3; j++)
		for (int r = 0; r < 3; r++) {
			float x = p.matrix.m[j * 4 + r];
			if (c) x = c->m[r] * p.matrix.m[j * 4] + c->m[4 + r] * p.matrix.m[j * 4 + 1] + c->m[8 + r] * p.matrix.m[j * 4 + 2];
			const float e = x - m.matrix.m[j * 4 + r];
			d += e * e;
		}
	return d;
}

float dist2(const Model &m)
{
	return m.matrix.m[12] * m.matrix.m[12] + m.matrix.m[13] * m.matrix.m[13] + m.matrix.m[14] * m.matrix.m[14];
}

// a model's shape: its meshes and triangles
uint64_t shape_of(const Model &m)
{
	uint64_t n = m.meshes ? m.meshes->size() : 0, verts = 0;
	if (m.meshes)
		for (auto &mesh : *m.meshes) verts += mesh.verts.size();
	return (n << 32) ^ verts;
}

} // namespace

// The camera's move between two frames' viewport (b = camera a, for what stands still), from the models alone at
// their address in both: the one most of the others agree with (the fighters move on their own)
bool camera_move(const Viewport &va, const Viewport &vb, Mat4 &camera)
{
	const std::vector<Model> &was = va.models, &now = vb.models;
	std::unordered_map<uint32_t, int> n_was, n_now;
	for (auto &m : was) n_was[m.addr]++;
	for (auto &m : now) n_now[m.addr]++;
	std::vector<std::pair<int, int>> sure;
	{
		std::unordered_map<uint32_t, int> at;
		for (size_t i = 0; i < was.size(); i++)
			if (n_was[was[i].addr] == 1) at[was[i].addr] = int(i);
		for (size_t j = 0; j < now.size(); j++)
			if (n_now[now[j].addr] == 1)
				if (auto it = at.find(now[j].addr); it != at.end()) sure.push_back({ it->second, int(j) });
	}
	// Only the farther half count, by their distance: most of the models alone at their address are the fighters'
	// parts, near the camera and moving together, and the stage's far parts show the camera's turn
	{
		std::vector<float> d;
		for (auto &q : sure) d.push_back(dist2(now[size_t(q.second)]));
		if (d.size() < 3) return false;
		std::nth_element(d.begin(), d.begin() + d.size() / 2, d.end());
		const float median = d[d.size() / 2];
		sure.erase(std::remove_if(sure.begin(), sure.end(), [&](const std::pair<int, int> &q) { return dist2(now[size_t(q.second)]) < median; }), sure.end());
	}
	camera = Mat4::identity();
	double best = 0, all = 0;
	size_t best_n = 0;
	for (auto &q : sure) all += std::sqrt(dist2(now[size_t(q.second)]));
	const size_t step = std::max<size_t>(1, sure.size() / 48);
	for (size_t k = 0; k < sure.size(); k += step) {
		Mat4 inv, c;
		if (!affine_inverse(was[size_t(sure[k].first)].matrix, inv)) continue;
		c = now[size_t(sure[k].second)].matrix * inv;
		double agree = 0;
		size_t n = 0;
		for (auto &q : sure)
			if (predicted2(c, was[size_t(q.first)], now[size_t(q.second)]) < 4e-4f * dist2(now[size_t(q.second)]) + 1e-2f) {
				agree += std::sqrt(dist2(now[size_t(q.second)]));
				n++;
			}
		if (agree > best) { best = agree; best_n = n; camera = c; }
	}
	return best_n >= 3 && best * 3 >= all;
}

namespace {

// b's model m's meshes with their vertices blended from p's (the same meshes made again)
// A model the game builds each frame (a fighter's joint, between two limbs): its vertices where they are between the
// two frames in the view (each from where a's matrix put it to where b's does), given in the blended matrix's terms.
// Blending the vertices and the matrix each on their own put them elsewhere when both change (a knee bending as the
// leg turns: the cross term), the joint off the limbs' ends, which blend exactly (a rigid model's vertex goes from a's
// place to b's): Lau's trousers in pieces for a picture as he changed stance.
void blend_vertices(const Model &p, Model &m, float t)
{
	const float *A = p.matrix.m, *B = m.matrix.m;
	float M[16];
	for (int i = 0; i < 16; i++) M[i] = A[i] + (B[i] - A[i]) * t;
	// the blended matrix's 3x3 inverse (column-major: column c, row r at c * 4 + r)
	auto at = [&](int r, int c) { return M[c * 4 + r]; };
	const float det = at(0, 0) * (at(1, 1) * at(2, 2) - at(1, 2) * at(2, 1)) - at(0, 1) * (at(1, 0) * at(2, 2) - at(1, 2) * at(2, 0)) +
	                  at(0, 2) * (at(1, 0) * at(2, 1) - at(1, 1) * at(2, 0));
	const bool view = std::fabs(det) > 1e-12f;
	float inv[3][3] = {};
	if (view) {
		const float k = 1.0f / det;
		inv[0][0] = (at(1, 1) * at(2, 2) - at(1, 2) * at(2, 1)) * k;
		inv[0][1] = (at(0, 2) * at(2, 1) - at(0, 1) * at(2, 2)) * k;
		inv[0][2] = (at(0, 1) * at(1, 2) - at(0, 2) * at(1, 1)) * k;
		inv[1][0] = (at(1, 2) * at(2, 0) - at(1, 0) * at(2, 2)) * k;
		inv[1][1] = (at(0, 0) * at(2, 2) - at(0, 2) * at(2, 0)) * k;
		inv[1][2] = (at(0, 2) * at(1, 0) - at(0, 0) * at(1, 2)) * k;
		inv[2][0] = (at(1, 0) * at(2, 1) - at(1, 1) * at(2, 0)) * k;
		inv[2][1] = (at(0, 1) * at(2, 0) - at(0, 0) * at(2, 1)) * k;
		inv[2][2] = (at(0, 0) * at(1, 1) - at(0, 1) * at(1, 0)) * k;
	}
	// x in the view: a's (or b's) matrix on a model-space vector (w 1 for a point, 0 for a direction)
	auto apply = [](const float *X, const float *v, float w, float *o) {
		for (int r = 0; r < 3; r++) o[r] = X[r] * v[0] + X[4 + r] * v[1] + X[8 + r] * v[2] + X[12 + r] * w;
	};
	auto blend = [&](const float *from, float *to, float w) {
		if (!view) {   // (a matrix that flattens it: as before)
			for (int c = 0; c < 3; c++) to[c] = from[c] + (to[c] - from[c]) * t;
			return;
		}
		float x[3], y[3], z[3];
		apply(A, from, w, x);
		apply(B, to, w, y);
		for (int c = 0; c < 3; c++) z[c] = x[c] + (y[c] - x[c]) * t - M[12 + c] * w;
		for (int r = 0; r < 3; r++) to[r] = inv[r][0] * z[0] + inv[r][1] * z[1] + inv[r][2] * z[2];
	};
	auto blended = std::make_shared<std::vector<Mesh>>(*m.meshes);
	for (size_t k = 0; k < blended->size(); k++) {
		const std::vector<Vertex> &from = (*p.meshes)[k].verts;
		std::vector<Vertex> &to = (*blended)[k].verts;
		for (size_t i = 0; i < to.size(); i++) {
			blend(from[i].pos, to[i].pos, 1);
			blend(from[i].normal, to[i].normal, 0);
			blend(from[i].face_normal, to[i].face_normal, 0);
		}
	}
	m.meshes = blended;
}

// The pairing of two frames' models, kept for the next picture between the same two (at 120 a second there are two;
// the camera's move and the pairing don't depend on where between them a picture is): for each of b's models, a's it
// is blended from (-1: shown as it is in b) and whether its vertices are too
// A flipbook (Shun's harbour water: a 64x64 picture and wave shapes that step every 3 frames, ~19 a second): a model
// whose texture moved to another place since the frame before, its shapes the same in number. Its last step is
// remembered (from: the model as it was before it) with the frames since (age); once two steps have come at most 8
// frames apart (period), each picture shows the step faded in from the one before over a period, a period late: it
// moves at the pictures' rate, not 19 steps a second. A step that doesn't repeat (a face's texture changed once) isn't.
struct Flip {
	Meshes from;
	int age = -1;      // frames since the step (-1: none remembered)
	int period = 0;    // frames between its last two steps (0: not known yet)
};
bool flip_step(const Model &p, const Model &m)
{
	if (!p.meshes || !m.meshes || p.meshes == m.meshes || p.meshes->size() != m.meshes->size()) return false;
	bool moved = false;
	for (size_t k = 0; k < m.meshes->size(); k++) {
		const Mesh &a = (*p.meshes)[k], &c = (*m.meshes)[k];
		if (a.verts.size() != c.verts.size() || a.textured != c.textured) return false;
		if (c.textured && a.width == c.width && a.height == c.height && a.page == c.page && a.format == c.format && (a.x != c.x || a.y != c.y)) moved = true;
	}
	return moved;
}

struct Plan {
	uint64_t a = 0, b = 0;
	bool ok = false;
	InterpStats st;
	std::vector<int> av;                     // a's viewport each of b's is paired with (-1: none)
	std::vector<std::vector<int>> from;
	std::vector<std::vector<int>> verts;     // a's model each of b's vertices are blended from, + 1 (0: not blended)
	std::vector<std::vector<int>> paired;    // every one of b's models' pair in a (blended or not)
	std::vector<std::vector<Flip>> flips;    // per b's model
	std::vector<std::vector<char>> swarm;    // b's models shown as a swarm's grains (snap 4)
};
Plan plan;
// the addresses a swarm's grains were seen at, and the frames they stay a swarm's (interpolate)
std::unordered_map<uint32_t, int> swarms;
constexpr int SWARM_FRAMES = 115;

// the flipbooks' steps faded in (after the rest of the blend): their shapes and the two pictures, by where between its
// frames the picture is
void apply_flips(const Scene &b, float t, Scene &out)
{
	static const bool off = getenv("VF3_NO_FLIPS") != nullptr;   // (tests: as before)
	if (off) return;
	for (size_t v = 0; v < out.viewports.size() && v < plan.flips.size(); v++)
		for (size_t j = 0; j < out.viewports[v].models.size() && j < plan.flips[v].size(); j++) {
			const Flip &f = plan.flips[v][j];
			// A particle or snapped model must not be re-blended by this later pass.
			if (v >= plan.from.size() || j >= plan.from[v].size() || plan.from[v][j] < 0 ||
			    (v < plan.swarm.size() && j < plan.swarm[v].size() && plan.swarm[v][j])) continue;
			if (f.age < 0 || f.period <= 0) continue;
			const float w = (float(f.age) + t) / float(f.period);   // (the newer step's share)
			if (w >= 1) continue;
			const Meshes &to = b.viewports[v].models[j].meshes;
			if (!to || !f.from || to->size() != f.from->size()) continue;
			auto blended = std::make_shared<std::vector<Mesh>>(*to);
			for (size_t k = 0; k < blended->size(); k++) {
				Mesh &n = (*blended)[k];
				const Mesh &o = (*f.from)[k];
				if (n.verts.size() != o.verts.size()) continue;
				for (size_t i = 0; i < n.verts.size(); i++)
					for (int c = 0; c < 3; c++) {
						n.verts[i].pos[c] = o.verts[i].pos[c] + (n.verts[i].pos[c] - o.verts[i].pos[c]) * w;
						n.verts[i].normal[c] = o.verts[i].normal[c] + (n.verts[i].normal[c] - o.verts[i].normal[c]) * w;
						n.verts[i].face_normal[c] = o.verts[i].face_normal[c] + (n.verts[i].face_normal[c] - o.verts[i].face_normal[c]) * w;
					}
				if (n.textured && (o.x != n.x || o.y != n.y) && o.width == n.width && o.height == n.height && o.page == n.page) {
					n.flip = 1 - w;
					n.flip_x = o.x;
					n.flip_y = o.y;
				}
			}
			out.viewports[v].models[j].meshes = blended;
		}
}

// A swarm's grain (the beach's spray, Wolf's desert sand: 1 to 6 polygons): under 40 vertices
bool grain(const Model &m)
{
	size_t n = 0;
	if (m.meshes)
		for (auto &mesh : *m.meshes) n += mesh.verts.size();
	return n < 40;
}

// How far the scene moved between a and b: the median move, over its distance, of the models alone at their address
// in both (not a swarm's grains) in the viewports paired (av: a's for each of b's); -1 if under 8 of them
float scene_move(const Scene &a, const Scene &b, const std::vector<int> &av)
{
	std::vector<float> moved;
	for (size_t v = 0; v < av.size(); v++) {
		if (av[v] < 0) continue;
		const std::vector<Model> &was = a.viewports[size_t(av[v])].models, &now = b.viewports[v].models;
		std::unordered_map<uint32_t, int> n_was, n_now;
		std::unordered_map<uint32_t, const Model *> at;
		for (auto &m : was) { n_was[m.addr]++; at[m.addr] = &m; }
		for (auto &m : now) n_now[m.addr]++;
		for (auto &m : now)
			if (n_now[m.addr] == 1 && n_was[m.addr] == 1 && !grain(m))
				moved.push_back(std::sqrt(moved2(*at[m.addr], m) / std::max(dist2(m), 1.0f)));
	}
	if (moved.size() < 8) return -1;
	std::nth_element(moved.begin(), moved.begin() + moved.size() / 2, moved.end());
	return moved[moved.size() / 2];
}

// A model's size: the square of its farthest vertex from its origin, in the view's units
float radius2(const Model &m)
{
	float r = 0;
	if (m.meshes)
		for (auto &mesh : *m.meshes)
			for (auto &v : mesh.verts) r = std::max(r, v.pos[0] * v.pos[0] + v.pos[1] * v.pos[1] + v.pos[2] * v.pos[2]);
	const float *c = m.matrix.m;
	return r * (c[0] * c[0] + c[1] * c[1] + c[2] * c[2]);
}

// Which of a's viewports each of b's is (-1: none); false for a cut. The same number: the same, in order. Otherwise
// by the models they have in common (their addresses), the most alike first, each once, at least half of the larger's
// in common: the game puts viewports in the list and takes them out (6 small ones in a throw at the beach, the ones
// after them moved along), and the whole scene was shown unblended for a frame, a stop then a jump in the fighters'
// motion. It does at cuts too (the stage's models the same, from elsewhere): a cut unless the scene moved under 10%
// (scene_move: a throw's 2% at most; a cut's 15-40%).
bool pair_viewports(const Scene &a, const Scene &b, std::vector<int> &av)
{
	av.assign(b.viewports.size(), -1);
	if (a.viewports.size() == b.viewports.size()) {
		for (size_t v = 0; v < av.size(); v++) av[v] = int(v);
		return true;
	}
	std::vector<std::tuple<float, int, int>> all;
	for (size_t u = 0; u < a.viewports.size(); u++) {
		std::unordered_map<uint32_t, int> n_was;
		for (auto &m : a.viewports[u].models) n_was[m.addr]++;
		for (size_t v = 0; v < b.viewports.size(); v++) {
			std::unordered_map<uint32_t, int> left = n_was;
			size_t common = 0;
			for (auto &m : b.viewports[v].models)
				if (auto it = left.find(m.addr); it != left.end() && it->second > 0) { it->second--; common++; }
			const size_t most = std::max(a.viewports[u].models.size(), b.viewports[v].models.size());
			if (common * 2 >= most && common) all.push_back({ -float(common) / float(most), int(u), int(v) });
		}
	}
	std::sort(all.begin(), all.end());
	std::vector<char> taken(a.viewports.size(), 0);
	for (auto &[score, u, v] : all)
		if (!taken[size_t(u)] && av[size_t(v)] < 0) { av[size_t(v)] = u; taken[size_t(u)] = 1; }
	const float moved = scene_move(a, b, av);
	return moved >= 0 && moved < 0.1f;
}

} // namespace

const std::vector<std::vector<int>> *interp_paired(const Scene &a, const Scene &b, std::vector<int> *viewports)
{
	if (!(a.id && plan.a == a.id && plan.b == b.id && plan.paired.size() == b.viewports.size())) return nullptr;
	if (viewports) *viewports = plan.av;
	return &plan.paired;
}

bool interpolate(const Scene &a, const Scene &b, float t, Scene &out, InterpStats *st)
{
	out = b;
	std::vector<int> av;
	if (a.blank || b.blank || !pair_viewports(a, b, av))
		return false;
	static const bool no_plan = getenv("VF3_NO_INTERP_PLAN") != nullptr;   // (tests: every picture paired afresh)
	if (!no_plan && a.id && b.id && plan.a == a.id && plan.b == b.id && plan.from.size() == out.viewports.size()) {
		if (st) *st = plan.st;
		if (!plan.ok) { out = b; return false; }
		for (size_t v = 0; v < out.viewports.size(); v++) {
			if (plan.av[v] < 0) continue;
			const std::vector<Model> &was = a.viewports[size_t(plan.av[v])].models;
			std::vector<Model> &now = out.viewports[v].models;
			for (size_t j = 0; j < now.size() && j < plan.from[v].size(); j++) {
				const int i = plan.from[v][j];
				if (i < 0) continue;
				const Model &p = was[size_t(i)];
				Model &m = now[j];
				if (plan.verts[v][j]) blend_vertices(was[size_t(plan.verts[v][j] - 1)], m, t);
				for (int k = 0; k < 16; k++) m.matrix.m[k] = p.matrix.m[k] + (m.matrix.m[k] - p.matrix.m[k]) * t;
				m.alpha = p.alpha + (m.alpha - p.alpha) * t;
			}
		}
		apply_flips(b, t, out);
		return true;
	}
	Plan np;
	np.a = a.id;
	np.b = b.id;
	InterpStats local;
	if (!st) st = &local;
	*st = InterpStats();
	size_t total = 0, matched = 0, voters = 0, voted = 0, strays = 0;
	for (auto it = swarms.begin(); it != swarms.end();) it = --it->second <= 0 ? swarms.erase(it) : std::next(it);
	np.av = std::move(av);
	for (size_t v = 0; v < out.viewports.size(); v++) {
		np.from.emplace_back(out.viewports[v].models.size(), -1);
		np.verts.emplace_back(out.viewports[v].models.size(), 0);
		np.paired.emplace_back(out.viewports[v].models.size(), -1);
		const int u = np.av[v];
		if (u < 0) {   // (a viewport new this frame: its models shown as they are)
			np.swarm.emplace_back(out.viewports[v].models.size(), 0);
			for (auto &m : out.viewports[v].models) voters += !grain(m);
			total += out.viewports[v].models.size();
			st->no_key += out.viewports[v].models.size();
			continue;
		}
		const Viewport &va = a.viewports[size_t(u)];
		const std::vector<Model> &was = va.models;
		std::vector<Model> &now = out.viewports[v].models;
		std::vector<char> taken(was.size(), 0);
		std::vector<int> pair(now.size(), -1);
		std::unordered_map<uint32_t, int> n_was, n_now;
		for (auto &m : was) n_was[m.addr]++;
		for (auto &m : now) n_now[m.addr]++;
		Mat4 camera;
		const bool have_camera = camera_move(va, b.viewports[v], camera);
		// how far a's model is from b's: from where the camera's move puts it, if known
		auto apart2 = [&](const Model &p, const Model &m) { return have_camera ? predicted2(camera, p, m) : moved2(p, m); };
		// The nearest of a's models with the same key not yet paired (several share a model's address: a stage's
		// palms, the fighters' limbs; the culling nodes above them are rebuilt elsewhere most frames, so they don't
		// tell them apart: in order, a model would be paired with its neighbour and drawn halfway to it)
		// (the nearest pairs of all of a key's first: taken in the drawing order, one could take another's nearest, the
		// two small flags on a tower each drawn halfway to the other)
		std::vector<char> crowded(now.size(), 0);   // (paired among several of its key)
		// An address pair more than 0.5% of its distance from where it would be (the camera's move, or none known: where
		// it was), when another model of its shape is 4 times nearer: left for the shape pass. Animations step through the same
		// models in ROM: a tower's two small flags, one this frame in the model the other had in the last, and a flock
		// of birds on Pai's rooftop sharing theirs; paired by address, each was drawn halfway to another. (Only with
		// another much nearer: a fighter's limb moves far on its own.)
		// (each model's shape once, and a's models by shape: looked through for each of b's models far from where it
		// would be, which on a big camera move is most of them, every one of a's models with its meshes counted made
		// a picture 3-4 ms)
		std::vector<uint64_t> was_shape(was.size()), now_shape(now.size());
		for (size_t i = 0; i < was.size(); i++) was_shape[i] = shape_of(was[i]);
		for (size_t j = 0; j < now.size(); j++) now_shape[j] = shape_of(now[j]);
		std::unordered_map<uint64_t, std::vector<int>> was_by_shape;
		for (size_t i = 0; i < was.size(); i++) was_by_shape[was_shape[i]].push_back(int(i));
		auto elsewhere = [&](int i, int j) {
			const Model &m = now[size_t(j)];
			const float own = apart2(was[size_t(i)], m);
			if (own <= 2.5e-5f * dist2(m) + 1e-2f) return false;
			auto it = was_by_shape.find(now_shape[size_t(j)]);
			if (it == was_by_shape.end() || it->second.size() > 32) return false;   // (a swarm of hundreds: each looked through all, a ms a picture)
			for (int k : it->second)
				if (!taken[size_t(k)] && k != i && apart2(was[size_t(k)], m) * 16 < own) return true;
			return false;
		};
		auto pair_by = [&](auto key, bool by_addr) {
			std::unordered_map<uint64_t, std::vector<int>> by, by_now;
			for (size_t i = 0; i < was.size(); i++)
				if (!taken[i]) by[key(was[i])].push_back(int(i));
			for (size_t j = 0; j < now.size(); j++)
				if (pair[j] < 0) by_now[key(now[j])].push_back(int(j));
			for (auto &[k, js] : by_now) {
				auto it = by.find(k);
				if (it == by.end()) continue;
				const std::vector<int> &is = it->second;
				if (is.size() == 1 && js.size() == 1) {
					if (elsewhere(is[0], js[0])) continue;
					pair[size_t(js[0])] = is[0];
					taken[size_t(is[0])] = 1;
					continue;
				}
				// (by place and by the way each points: a fighter's trouser segments share an address and nearly their
				// place, at the hip; by place alone one was paired with its neighbour, pointing another way, and drawn
				// halfway to it, the legs in pieces for a picture. Pointing counts as much as a tenth of the distance.)
				// A big group (a swarm of dust or sand, hundreds of one shape): each of b's, in turn, with the nearest of a's
				// not yet taken. Every pair sorted was the square of them, 5-9 ms a picture on a Steam Deck in Wolf's
				// desert; a dust grain paired with its neighbour's last place isn't seen.
				if (is.size() * js.size() > 1024) {
					for (int j : js) {
						int best = -1;
						float best_d = 0;
						for (int i : is) {
							if (taken[size_t(i)]) continue;
							const float d = apart2(was[size_t(i)], now[size_t(j)]);
							if (best < 0 || d < best_d) { best = i; best_d = d; }
						}
						if (best >= 0 && !(by_addr && elsewhere(best, j))) { pair[size_t(j)] = best; taken[size_t(best)] = 1; crowded[size_t(j)] = 1; }
					}
					continue;
				}
				std::vector<std::tuple<float, int, int>> all;
				for (int j : js)
					for (int i : is) {
						const Model &p = was[size_t(i)], &m = now[size_t(j)];
						all.push_back({ apart2(p, m) + turned2(have_camera ? &camera : nullptr, p, m) * 0.01f * dist2(m), i, j });
					}
				std::sort(all.begin(), all.end());
				for (auto &[d, i, j] : all)
					if (!taken[size_t(i)] && pair[size_t(j)] < 0 && !(by_addr && elsewhere(i, j))) { pair[size_t(j)] = i; taken[size_t(i)] = 1; crowded[size_t(j)] = 1; }
				static const bool pair_log = getenv("VF3_PAIR_LOG") != nullptr;
				if (pair_log && is.size() <= 6) {   // (tests)
					fprintf(stderr, "pair group cam %d:", have_camera);
					for (int j : js) {
						const Model &mm = now[size_t(j)];
						fprintf(stderr, " [now %08x (%.0f %.0f %.0f) <- %d", mm.addr, mm.matrix.m[12], mm.matrix.m[13], mm.matrix.m[14], pair[size_t(j)]);
						if (pair[size_t(j)] >= 0) { const Model &pp = was[size_t(pair[size_t(j)])]; fprintf(stderr, " %08x (%.0f %.0f %.0f) d %.1f", pp.addr, pp.matrix.m[12], pp.matrix.m[13], pp.matrix.m[14], std::sqrt(apart2(pp, mm))); }
						fprintf(stderr, "]");
					}
					fprintf(stderr, "\n");
				}
			}
		};
		pair_by([](const Model &m) { return uint64_t(m.addr); }, true);
		// then by shape: an animation's next model (the birds' wings, a splash: the game steps through models in
		// ROM), a model the game builds each frame (in polygon RAM, wherever there was room)
		pair_by([&](const Model &m) { return (&m >= now.data() && &m < now.data() + now.size()) ? now_shape[size_t(&m - now.data())] : was_shape[size_t(&m - was.data())]; }, false);
		// then by place: another model where one was (a detail level for another, the same nodes over it)
		for (size_t j = 0; j < now.size(); j++) {
			if (pair[j] >= 0) continue;
			float d0 = 0;
			for (int r = 0; r < 3; r++) d0 += now[j].matrix.m[12 + r] * now[j].matrix.m[12 + r];
			int best = -1;
			float best_d = 1e-6f * d0 + 1e-4f;
			for (size_t i = 0; i < was.size(); i++)
				if (!taken[i] && moved2(was[i], now[j]) < best_d) { best = int(i); best_d = moved2(was[i], now[j]); }
			if (best >= 0) { pair[j] = best; taken[best] = 1; }
		}
		if (const char *pn = getenv("VF3_PAIR_NEAR")) {   // (tests: "x,y,r": the models whose place is near (x, y) on screen, and their pairs)
			float tx = 0, ty = 0, tr = 0;
			sscanf(pn, "%f,%f,%f", &tx, &ty, &tr);
			const Viewport &vp = b.viewports[v];
			auto proj = [&](const Model &mm, float &sx, float &sy) {
				const float z = -mm.matrix.m[14];
				if (z <= 0) return false;
				sx = vp.vp_x + vp.vp_w * (mm.matrix.m[12] / z - vp.left) / (vp.right - vp.left);
				sy = vp.vp_y + vp.vp_h * (vp.top - mm.matrix.m[13] / z) / (vp.top - vp.bottom);
				return true;
			};
			for (size_t j = 0; j < now.size(); j++) {
				float sx, sy;
				if (!proj(now[j], sx, sy) || std::fabs(sx - tx) > tr || std::fabs(sy - ty) > tr) continue;
				fprintf(stderr, "near vp %zu now %zu %08x at %.0f,%.0f", v, j, now[j].addr, sx, sy);
				if (pair[j] >= 0) {
					float px, py;
					proj(was[size_t(pair[j])], px, py);
					fprintf(stderr, " <- %d %08x at %.0f,%.0f crowded %d", pair[j], was[size_t(pair[j])].addr, px, py, int(crowded[j]));
				}
				fprintf(stderr, "\n");
				if (pair[j] >= 0 && getenv("VF3_PAIR_MAT")) {
					const Model &pp = was[size_t(pair[j])];
					for (int k = 0; k < 16; k++) fprintf(stderr, " %.2f/%.2f", pp.matrix.m[k], now[j].matrix.m[k]);
					fprintf(stderr, " meshes %zu/%zu same %d\n", pp.meshes ? pp.meshes->size() : 0, now[j].meshes ? now[j].meshes->size() : 0, int(pp.meshes == now[j].meshes));
				}
			}
			for (size_t i = 0; i < was.size(); i++) {
				float sx, sy;
				if (!proj(was[i], sx, sy) || std::fabs(sx - tx) > tr || std::fabs(sy - ty) > tr) continue;
				fprintf(stderr, "near vp %zu was %zu %08x at %.0f,%.0f taken %d\n", v, i, was[i].addr, sx, sy, int(taken[i]));
			}
		}
		// too far in a frame: shown where it is now (0 blended, 1 turned, 2 moved, 3 another of several, 4 a swarm's grain)
		std::vector<uint8_t> snap(now.size(), 0);
		std::vector<char> loose(now.size(), 0), far(now.size(), 0);
		for (size_t j = 0; j < now.size(); j++) {
			if (pair[j] < 0) continue;
			const Model &m = now[j], &p = was[size_t(pair[j])];
			float rot = 0, dist = 0;
			for (int c = 0; c < 3; c++)
				for (int r = 0; r < 3; r++) rot = std::max(rot, std::fabs(p.matrix.m[c * 4 + r] - m.matrix.m[c * 4 + r]));
			for (int r = 0; r < 3; r++) dist += m.matrix.m[12 + r] * m.matrix.m[12 + r];
			const float move = moved2(p, m);
			// One of several sharing an address (a stage's flags) far from where the camera's move puts the one it
			// was paired with (more than 15% of its distance): another of them, not blended (it was drawn halfway
			// between the two, a flag in the sky for a picture)
			const bool wrong = (n_was[p.addr] > 1 || n_now[m.addr] > 1 || crowded[j]) && apart2(p, m) > 0.0225f * dist + 1.0f;
			static const bool no_crowded = getenv("VF3_NO_CROWDED") != nullptr;   // (tests: crowded pairs not blended)
			// (a swarm's grain not alone at its address in both frames, far from where the camera's move puts its
			// partner: below)
			loose[j] = grain(m) && !(p.addr == m.addr && n_was[p.addr] == 1 && n_now[m.addr] == 1);
			far[j] = loose[j] && apart2(p, m) > std::max(4 * radius2(m), 0.0004f * dist);
			snap[j] = rot > 0.75f ? 1 : move > 0.25f * dist + 1.0f ? 2 : wrong || (no_crowded && crowded[j]) ? 3 : 0;
		}
		// A swarm's grains (Wolf's desert sand, the beach's spray: kicked up and put somewhere new each frame, dozens to an
		// address): blended, each was drawn along a line between it and another grain, the sand around a fighter's feet
		// swirling and flickering at 120 a second. Shown where they are, as the arcade's frames have them. An address is
		// a swarm's when most of its grains not alone at it are further from where the camera's move puts their partners
		// than twice their size and 2% of their distance, and stays one for 2 seconds after, as does a grain whose partner
		// was one: by the grain, a swarm's near its partner by chance glided one frame and jumped the next. (A grain alone
		// at its address, a spark, glides; so do a stage's small models that share one, flags, birds, a fighter's hair,
		// which stay near where the camera's move puts them.)
		{
			std::unordered_map<uint32_t, std::pair<int, int>> group;   // (grains not alone at the address, and of them far)
			for (size_t j = 0; j < now.size(); j++)
				if (loose[j]) { auto &g = group[now[j].addr]; g.first++; g.second += far[j]; }
			for (auto &[addr, g] : group)
				if (g.first >= 2 && g.second * 2 > g.first) swarms[addr] = SWARM_FRAMES;
			// (or its partner was one the frame before: a grain's next step of its animation is at another address)
			const bool chain = plan.b == a.id && a.id && size_t(u) < plan.swarm.size();
			np.swarm.emplace_back(now.size(), 0);
			for (size_t j = 0; j < now.size(); j++) {
				const bool was_one = chain && pair[j] >= 0 && size_t(pair[j]) < plan.swarm[size_t(u)].size() && plan.swarm[size_t(u)][size_t(pair[j])];
				if (loose[j] && (snap[j] == 0 || snap[j] == 3) && (far[j] || was_one || swarms.count(now[j].addr))) snap[j] = 4;
				np.swarm[v][j] = snap[j] == 4;
			}
		}
		for (size_t j = 0; j < now.size(); j++) {
			Model &m = now[j];
			total++;
			strays += snap[j] == 4;
			voters += !grain(m);
			np.paired[v][j] = pair[j];
			if (pair[j] < 0) { if (st) st->no_key++; continue; }
			const Model &p = was[size_t(pair[j])];
			if (snap[j]) {
				if (st) (snap[j] == 1 ? st->turned : st->moved)++;
				continue;
			}
			matched++;
			voted += !grain(m);
			np.from[v][j] = pair[j];
			int matrix_from = pair[j];   // (the partner its vertices are blended from, when another: its matrix too)
			// A model the game builds each frame (the fighters' joints, between their limbs): its vertices blended
			// too, when it is the same model (its meshes made the same way, with the same textures: their texture
			// coordinates may change, a shiny surface's are made each frame), or it stays where it is in b while the
			// limbs move between. Another model built in the same place (one face for another) isn't blended into it.
			if (m.meshes && p.meshes && m.meshes != p.meshes && m.meshes->size() == p.meshes->size()) {
				// The same model made the same way (its meshes with the same textures), and how far its vertices are
				// from b's in the view (the farthest one, squared; size: b's across)
				auto alike = [&](const Model &q) {
					if (!q.meshes || q.meshes->size() != m.meshes->size()) return false;
					for (size_t k = 0; k < m.meshes->size(); k++) {
						const Mesh &x = (*m.meshes)[k], &y = (*q.meshes)[k];
						if (!(x.verts.size() == y.verts.size() && x.key == y.key && x.x == y.x && x.y == y.y && x.page == y.page &&
						      x.width == y.width && x.height == y.height && x.format == y.format)) return false;
					}
					return true;
				};
				float size = 0;
				auto apart = [&](const Model &q, bool &uv_same) {
					float most = 0, lo[3] = { 1e30f, 1e30f, 1e30f }, hi[3] = { -1e30f, -1e30f, -1e30f };
					uv_same = true;
					for (size_t k = 0; k < m.meshes->size(); k++) {
						const std::vector<Vertex> &x = (*m.meshes)[k].verts, &y = (*q.meshes)[k].verts;
						for (size_t i = 0; i < x.size(); i++) {
							uv_same &= x[i].uv[0] == y[i].uv[0] && x[i].uv[1] == y[i].uv[1];
							float d = 0;
							for (int r = 0; r < 3; r++) {
								const float *u = y[i].pos, *w = x[i].pos;
								const float e = q.matrix.m[r] * u[0] + q.matrix.m[4 + r] * u[1] + q.matrix.m[8 + r] * u[2] + q.matrix.m[12 + r];
								const float f = m.matrix.m[r] * w[0] + m.matrix.m[4 + r] * w[1] + m.matrix.m[8 + r] * w[2] + m.matrix.m[12 + r];
								d += (e - f) * (e - f);
								lo[r] = std::min(lo[r], f);
								hi[r] = std::max(hi[r], f);
							}
							most = std::max(most, d);
						}
					}
					size = 0;
					for (int r = 0; r < 3; r++) size += (hi[r] - lo[r]) * (hi[r] - lo[r]);
					return most;
				};
				int from = -1;
				bool uv_same = false;
				float best = 0;
				if (alike(p)) { from = pair[j]; best = apart(p, uv_same); }
				const bool by_place = from >= 0;
				// Another of a's models of its shape, made the same way, with its vertices 4 times nearer: the one it is.
				// The game makes the fighters' joints each frame wherever there is room, and both knees use the same
				// texture: the left knee made where the right one was, paired by its place, was blended into the other
				// (the same texture coordinates passed it), Lau's trousers in pieces for a picture as he changed stance.
				// (only when its place's partner is a quarter of its size or more away, and among a few: the knees are 2
				// to 4 of a shape; a swarm of sand or splashes is dozens, and looking through them all for each of them
				// took a picture 24 ms on a Steam Deck)
				if (auto it = was_by_shape.find(now_shape[j]); it != was_by_shape.end() && it->second.size() <= 8 && (from < 0 || best * 16 >= size))
					for (int k : it->second) {
						const Model &q = was[size_t(k)];
						if (k == pair[j] || !q.meshes || q.meshes == m.meshes || !alike(q)) continue;
						bool us = false;
						const float d = apart(q, us);
						if (from < 0 || (by_place ? d * 16 < best : d < best)) { from = k; best = d; uv_same = us; }   // (with no place partner: the nearest)
					}
				// The same texture coordinates, or (a shiny surface's are made each frame) every vertex moved, its move less
				// the model's average move, less than a quarter of the mesh's size: a joint bends and goes with the limbs
				// (Lau's shiny trousers moved 300-765 against sizes of 380-785 as he switched stance, nearly all of it
				// together: by the whole move they weren't blended while the rigid pieces between them were, his legs in
				// pieces for a picture), where one face put in another's place (the select screen's portraits, a face
				// shutting its eyes) moves its vertices every which way, the average little.
				auto bent = [&](const Model &q) {
					double mean[3] = { 0, 0, 0 };
					size_t n = 0;
					auto moved = [&](size_t k, size_t i, float *d) {
						const float *u = (*q.meshes)[k].verts[i].pos, *w = (*m.meshes)[k].verts[i].pos;
						for (int r = 0; r < 3; r++)
							d[r] = (m.matrix.m[r] * w[0] + m.matrix.m[4 + r] * w[1] + m.matrix.m[8 + r] * w[2] + m.matrix.m[12 + r]) -
							       (q.matrix.m[r] * u[0] + q.matrix.m[4 + r] * u[1] + q.matrix.m[8 + r] * u[2] + q.matrix.m[12 + r]);
					};
					for (size_t k = 0; k < m.meshes->size(); k++)
						for (size_t i = 0; i < (*m.meshes)[k].verts.size(); i++) {
							float d[3];
							moved(k, i, d);
							for (int r = 0; r < 3; r++) mean[r] += d[r];
							n++;
						}
					if (n) for (double &x : mean) x /= double(n);
					float most = 0;
					for (size_t k = 0; k < m.meshes->size(); k++)
						for (size_t i = 0; i < (*m.meshes)[k].verts.size(); i++) {
							float d[3], e = 0;
							moved(k, i, d);
							for (int r = 0; r < 3; r++) e += (d[r] - float(mean[r])) * (d[r] - float(mean[r]));
							most = std::max(most, e);
						}
					return most;
				};
				const bool same = from >= 0 && ((uv_same && by_place) || best < size / 16 || bent(was[size_t(from)]) < size / 16);   // (one found by its vertices: near them)
				if (st) (same ? st->blended : st->not_blended)++;
				if (same) {
					np.verts[v][j] = from + 1;
					blend_vertices(was[size_t(from)], m, t);
					// (its vertices are where they are in the view from that one's matrix: the matrix blended from it
					// too, or the joint was put off by the two partners' difference)
					matrix_from = from;
					np.from[v][j] = from;
				}
			}
			const Model &q = was[size_t(matrix_from)];
			for (int i = 0; i < 16; i++) m.matrix.m[i] = q.matrix.m[i] + (m.matrix.m[i] - q.matrix.m[i]) * t;
			m.alpha = q.alpha + (m.alpha - q.alpha) * t;
		}
	}
	st->total = total;
	st->matched = matched;
	np.st = *st;
	// Most models too far in a frame (or new): a cut, the whole scene shown as it is in b. But not when the rest, a
	// swarm's grains left out, are 32 or more and 70% of them blended: the game puts hundreds of grains somewhere new
	// each frame (Wolf's desert sand, the beach's spray around a fighter's feet: 400-900 of a scene's 900-1400 models,
	// or all put in at once), and they outvoted the rest, the whole scene unblended for frames on end as the fighters
	// moved, a stop then a jump each frame. (A real cut, and a thrown fighter's spin whose pieces turned too far, left
	// 58-63% of the rest blended: those stay cuts, the thrown fighter's pieces not drawn apart for a frame.)
	// And the scene moved over 25% of its distance in a frame (scene_move): a cut, whatever the models' vote (VF1's:
	// always a cut). The stage's models moved less than a quarter of their distance each, or are many of an address,
	// and most were blended across the attract's cuts to a fighter's introduction, his pieces flying across the screen.
	// (The grains shown where they are on purpose, 4, don't vote either way.)
	const size_t vote = total - strays;
	const bool cut = (vote && matched * 2 < vote && !(voters >= 32 && voted * 10 >= voters * 7)) || scene_move(a, b, np.av) > 0.25f;
	np.ok = !cut;
	// the flipbooks: a step from a to b remembered, or the one remembered for a's model carried on (when a's frame
	// was the last plan's b: one frame on)
	{
		const bool chain = np.ok && plan.ok && plan.b == a.id && a.id && plan.flips.size() == a.viewports.size();
		const std::vector<int> &av = np.av;
		np.flips.resize(np.paired.size());
		for (size_t v = 0; v < np.paired.size(); v++) {
			np.flips[v].resize(np.paired[v].size());
			for (size_t j = 0; j < np.paired[v].size(); j++) {
				const int i = np.paired[v][j];
				if (!np.ok || i < 0 || np.from[v][j] < 0 || np.swarm[v][j]) continue;
				const size_t u = size_t(av[v]);   // (paired: in a viewport of a's)
				const Flip *pr = chain && size_t(i) < plan.flips[u].size() && plan.flips[u][size_t(i)].age >= 0 ? &plan.flips[u][size_t(i)] : nullptr;
				Flip &f = np.flips[v][j];
				if (flip_step(a.viewports[u].models[size_t(i)], b.viewports[v].models[j])) {
					f.from = a.viewports[u].models[size_t(i)].meshes;
					f.age = 0;
					f.period = pr && pr->age + 1 >= 2 && pr->age + 1 <= 8 ? pr->age + 1 : 0;
				} else if (pr && pr->age + 1 <= 8) {
					f = *pr;
					f.age++;
				}
			}
		}
	}
	if (a.id && b.id) plan = std::move(np);
	if (cut) {
		out = b;
		return false;
	}
	if (a.id && b.id) apply_flips(b, t, out);
	return true;
}

} // namespace video
