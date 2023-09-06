#pragma once
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

typedef float vec2[2];
typedef float vec3[3];
typedef float vec4[4];

typedef float mat3[9];
typedef float mat4[16];

void vec3_copy(vec3 dst, const vec3 src);
float vec3_dot(const vec3 lhs, const vec3 rhs);
void vec3_cross(vec3 dst, const vec3 lhs, const vec3 rhs);
void vec3_normalize(vec3 dst);

void vec4_copy(vec4 dst, const vec4 src);
float vec4_dot(const vec4 lhs, const vec4 rhs);

void mat4_identity(mat4 dst);
void mat4_copy(mat4 dst, const mat4 src);
void mat4_multiply(mat4 dst, const mat4 lhs, const mat4 rhs);

void mat4_ortho(mat4 dst, const float left, const float right, const float bottom, const float top, const float near, const float far);
void mat4_frustum(mat4 dst, const float left, const float right, const float bottom, const float top, const float near, const float far);
void mat4_perspective(mat4 dst, const float fovy, const float aspect, const float near, const float far);
void mat4_lookat(mat4 dst, const vec3 eye, const vec3 center, const vec3 up);

void mat4_rotate(mat4 dst, const float angle, const vec3 axis);
void mat4_rotatex(mat4 dst, const float angle);
void mat4_rotatey(mat4 dst, const float angle);
void mat4_rotatez(mat4 dst, const float angle);
void mat4_scale(mat4 dst, const vec4 scale);
void mat4_translate(mat4 dst, const vec3 translate);
