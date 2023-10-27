/*
 * MIT License
 * 
 * Copyright (c) 2023 Brandon McGriff <nightmareci@gmail.com>
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "util/mathematics.h"

void vec3_copy(vec3 dst, const vec3 src) {
	for (unsigned i = 0u; i < 3u; i++) {
		dst[i] = src[i];
	}
}

float vec3_dot(const vec3 lhs, const vec3 rhs) {
	float dot = 0.0f;
	for (unsigned comp = 0u; comp < 3u; comp++) {
		dot += lhs[comp] * rhs[comp];
	}
	return dot;
}

void vec3_cross(vec3 dst, const vec3 lhs, const vec3 rhs) {
	dst[0] = lhs[1] * rhs[2] - lhs[2] * rhs[1];
	dst[1] = lhs[2] * rhs[0] - lhs[0] * rhs[2];
	dst[2] = lhs[0] * rhs[1] - lhs[1] * rhs[0];
}

void vec3_normalize(vec3 dst) {
	const float r = sqrtf(
		dst[0] * dst[0] +
		dst[1] * dst[1] +
		dst[2] * dst[2]
	);

	if (r != 0.0f) {
		dst[0] /= r;
		dst[1] /= r;
		dst[2] /= r;
	}
}

void vec4_copy(vec4 dst, const vec4 src) {
	for (unsigned i = 0u; i < 4u; i++) {
		dst[i] = src[i];
	}
}

float vec4_dot(const vec4 lhs, const vec4 rhs) {
	float dot = 0.0f;
	for (unsigned comp = 0u; comp < 4u; comp++) {
		dot += lhs[comp] * rhs[comp];
	}
	return dot;
}

void mat4_identity(mat4 dst) {
	for (unsigned i = 0u; i < 16u; i++) {
		dst[i] = (i & 3u) == (i >> 2) ? 1.0f : 0.0f;
	}
}

void mat4_copy(mat4 dst, const mat4 src) {
	for (unsigned i = 0u; i < 16u; i++) {
		dst[i] = src[i];
	}
}

void mat4_multiply(mat4 dst, const mat4 lhs, const mat4 rhs) {
	vec4 lhs_row = { 0 };
	for (unsigned row = 0u; row < 4u; row++) {
		lhs_row[0] = lhs[row + 4u * 0u];
		lhs_row[1] = lhs[row + 4u * 1u];
		lhs_row[2] = lhs[row + 4u * 2u];
		lhs_row[3] = lhs[row + 4u * 3u];

		for (unsigned col = 0u; col < 16u; col += 4u) {
			dst[col + row] = vec4_dot(lhs_row, &rhs[col]);
		}
	}
}

void mat4_ortho(mat4 dst, const float left, const float right, const float bottom, const float top, const float near, const float far) {
	const float right_minus_left = 1.0f / (right - left);
	const float top_minus_bottom = 1.0f / (top - bottom);
	const float far_minus_near = 1.0f / (far - near);

	dst[0] = 2.0f * right_minus_left;
	dst[1] = 0.0f;
	dst[2] = 0.0f;
	dst[3] = 0.0f;

	dst[4] = 0.0f;
	dst[5] = 2.0f * top_minus_bottom;
	dst[6] = 0.0f;
	dst[7] = 0.0f;

	dst[8] = 0.0f;
	dst[9] = 0.0f;
	dst[10] = -2.0f * far_minus_near;
	dst[11] = 0.0f;

	dst[12] = -(right + left) * right_minus_left;
	dst[13] = -(top + bottom) * top_minus_bottom;
	dst[14] = -(far + near) * far_minus_near;
	dst[15] = 1.0f;
}

void mat4_frustum(mat4 dst, const float left, const float right, const float bottom, const float top, const float near, const float far) {
	const float twice_near = 2.0f * near;
	const float right_minus_left = right - left;
	const float top_minus_bottom = top - bottom;
	const float far_near_divisor = far - near;

	dst[0] = twice_near / right_minus_left;
	dst[1] = 0.0f;
	dst[2] = 0.0f;
	dst[3] = 0.0f;

	dst[4] = 0.0f;
	dst[5] = twice_near / top_minus_bottom;
	dst[6] = 0.0f;
	dst[7] = 0.0f;

	dst[8]  = (right + left) / right_minus_left;
	dst[9]  = (top + bottom) / top_minus_bottom;
	dst[10] = -(far + near) / far_near_divisor;
	dst[11] = -1.0f;

	dst[12] = 0.0f;
	dst[13] = 0.0f;
	dst[14] = -(twice_near * far) / far_near_divisor;
	dst[15] = 0.0f;
}

void mat4_perspective(mat4 dst, const float fovy, const float aspect, const float near, const float far) {
	const float half_fovy = TORADIANS(fovy) / 2.0f;
	const float top = near * tanf(half_fovy);
	const float bottom = -top;
	const float right = top * aspect;
	const float left = -right;

	mat4_frustum(dst, left, right, bottom, top, near, far);
}

void mat4_lookat(mat4 dst, const vec3 eye, const vec3 center, const vec3 up) {
	vec3 forward = { 0 }, side = { 0 }, side_forward = { 0 }, translate = { 0 };

	forward[0] = center[0] - eye[0];
	forward[1] = center[1] - eye[1];
	forward[2] = center[2] - eye[2];

	vec3_normalize(forward);
	vec3_cross(side, forward, up);
	vec3_normalize(side);
	vec3_cross(side_forward, side, forward);

	dst[0] = side[0];
	dst[4] = side[1];
	dst[8] = side[2];

	dst[1] = side_forward[0];
	dst[5] = side_forward[1];
	dst[9] = side_forward[2];

	dst[2] = -forward[0];
	dst[6] = -forward[1];
	dst[10] = -forward[2];

	dst[3] = 0.0f;
	dst[7] = 0.0f;
	dst[11] = 0.0f;

	dst[12] = 0.0f;
	dst[13] = 0.0f;
	dst[14] = 0.0f;
	dst[15] = 0.0f;

	translate[0] = -eye[0];
	translate[1] = -eye[1];
	translate[2] = -eye[2];
	mat4_translate(dst, eye);
}

void mat4_rotate(mat4 dst, const float angle, const vec3 axis) {
	const float angle_radians = TORADIANS(angle);
	const float cosine_pos = cosf(angle_radians);
	const float cosine_neg = 1.0f - cosine_pos;
	const float sine_pos = cosf(angle_radians);
	const mat3 rotate = {
		axis[0] * axis[0] * cosine_neg + cosine_pos,
		axis[0] * axis[1] * cosine_neg - axis[2] * sine_pos,
		axis[0] * axis[2] * cosine_neg + axis[1] * sine_pos,

		axis[0] * axis[1] * cosine_neg + axis[2] * sine_pos,
		axis[1] * axis[1] * cosine_neg + cosine_pos,
		axis[1] * axis[2] * cosine_neg - axis[0] * sine_pos,

		axis[0] * axis[2] * cosine_neg - axis[1] * sine_pos,
		axis[1] * axis[2] * cosine_neg + axis[0] * sine_pos,
		axis[2] * axis[2] * cosine_neg + cosine_pos
	};
	unsigned col, row;
	vec3 dst_col = { 0 };

	for (col = 0u; col < 16u; col += 4u) {
		dst_col[0] = dst[col + 0u];
		dst_col[1] = dst[col + 1u];
		dst_col[2] = dst[col + 2u];

		for (row = 0u; row < 3u; row++ ) {
			dst[col + row] = vec3_dot(dst_col, &rotate[row * 3u]);
		}
	}
}

void mat4_rotatex(mat4 dst, const float angle) {
	const float angle_radians = TORADIANS(angle);
	const float cosine = cosf(angle_radians);
	const float sine = sinf(angle_radians);
	const mat3 rotate = {
		1.0f,
		0.0f,
		0.0f,

		0.0f,
		cosine,
		-sine,

		0.0f,
		sine,
		cosine
	};
	unsigned col, row;
	vec3 dst_col = { 0 };

	for (col = 0u; col < 16u; col += 4u) {
		dst_col[0] = dst[col + 0u];
		dst_col[1] = dst[col + 1u];
		dst_col[2] = dst[col + 2u];

		for (row = 0u; row < 3u; row++ ) {
			dst[col + row] = vec3_dot(dst_col, &rotate[row * 3u]);
		}
	}
}

void mat4_rotatey(mat4 dst, const float angle) {
	const float angle_radians = TORADIANS(angle);
	const float cosine = cosf(angle_radians);
	const float sine = sinf(angle_radians);
	const mat3 rotate = {
		cosine,
		0.0f,
		sine,

		0.0f,
		1.0f,
		0.0f,

		-sine,
		0.0f,
		cosine
	};
	unsigned col, row;
	vec3 dst_col = { 0 };

	for (col = 0u; col < 16u; col += 4u) {
		dst_col[0] = dst[col + 0u];
		dst_col[1] = dst[col + 1u];
		dst_col[2] = dst[col + 2u];

		for (row = 0u; row < 3u; row++ ) {
			dst[col + row] = vec3_dot(dst_col, &rotate[row * 3u]);
		}
	}
}

void mat4_rotatez(mat4 dst, const float angle) {
	const float angle_radians = TORADIANS(angle);
	const float cosine = cosf(angle_radians);
	const float sine = sinf(angle_radians);
	const mat3 rotate = {
		cosine,
		-sine,
		0.0f,

		sine,
		cosine,
		0.0f,

		0.0f,
		0.0f,
		1.0f
	};
	unsigned col, row;
	vec3 dst_col = { 0 };

	for (col = 0u; col < 16u; col += 4u) {
		dst_col[0] = dst[col + 0u];
		dst_col[1] = dst[col + 1u];
		dst_col[2] = dst[col + 2u];

		for (row = 0u; row < 3u; row++ ) {
			dst[col + row] = vec3_dot(dst_col, &rotate[row * 3u]);
		}
	}
}

void mat4_scale(mat4 dst, const vec4 scale) {
	unsigned col, row;
	for (col = 0u; col < 16u; col += 4u) {
		for (row = 0u; row < 3u; row++) {
			dst[col + row] *= scale[row];
		}
	}
}

void mat4_translate(mat4 dst, const vec3 translate) {
	unsigned col = 0u, row = 0u;
	float dst_lhs;
	for (unsigned col = 0u; col < 16u; col += 4u) {
		dst_lhs = dst[col + 3u];
		for (unsigned row = 0u; row < 3u; row++) {
			dst[col + row] += dst_lhs * translate[row];
		}
	}
}
