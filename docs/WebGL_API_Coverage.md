# PrismaUI WebGL API Coverage

WebGL 1.0 implementation status for PrismaUI's ANGLE-based WebGL bridge.

**WebGL 1.0 Coverage: 129 / 131 methods (~98%)**
**WebGL 2.0 Coverage: ~80 new methods implemented**

The implemented subset targets THREE.js and common WebGL tutorials. Functions
marked "full" are direct pass-throughs to ANGLE's GLES2 backend with no known
limitations.

---

## Canvas / Context

| Method | Status | Notes |
|---|---|---|
| `getContextAttributes` | **Yes** | Hardcoded attributes matching EGL config. |
| `isContextLost` | **Yes** | Always returns `false`; does not track actual device loss. |
| `getSupportedExtensions` | **Yes** | Hardcoded list (OES_element_index_uint, OES_standard_derivatives, OES_texture_float, OES_texture_half_float, WEBGL_depth_texture, WEBGL_lose_context). |
| `getExtension` | **Yes** | Returns empty object for known extensions; no extension-specific methods. |
| `drawingBufferWidth` | **Yes** | JS getter; returns `canvas.width`. |
| `drawingBufferHeight` | **Yes** | JS getter; returns `canvas.height`. |

## Viewing and Clipping

| Method | Status | Notes |
|---|---|---|
| `scissor` | **Yes** | Full. |
| `viewport` | **Yes** | Full. |

## State

| Method | Status | Notes |
|---|---|---|
| `activeTexture` | **Yes** | Full. |
| `blendColor` | **Yes** | Full. |
| `blendEquation` | **Yes** | Full. |
| `blendEquationSeparate` | **Yes** | Full. |
| `blendFunc` | **Yes** | Full. |
| `blendFuncSeparate` | **Yes** | Full. |
| `clearColor` | **Yes** | Full. |
| `clearDepth` | **Yes** | Full (`glClearDepthf`). |
| `clearStencil` | **Yes** | Full. |
| `colorMask` | **Yes** | Full. |
| `cullFace` | **Yes** | Full. |
| `depthFunc` | **Yes** | Full. |
| `depthMask` | **Yes** | Full. |
| `depthRange` | **Yes** | Full (`glDepthRangef`). |
| `disable` | **Yes** | Full. |
| `enable` | **Yes** | Full. |
| `frontFace` | **Yes** | Full. |
| `getError` | **Yes** | Full. |
| `getParameter` | **Yes** | Covers common pnames (strings, ints, booleans, typed arrays). Unknown pnames fall through to `glGetIntegerv`. |
| `hint` | **Yes** | Full. |
| `isEnabled` | **Yes** | Full. |
| `lineWidth` | **Yes** | Full (many drivers clamp to 1.0). |
| `pixelStorei` | **Yes** | Full. |
| `polygonOffset` | **Yes** | Full. |
| `sampleCoverage` | **Yes** | Full. |
| `stencilFunc` | **Yes** | Full. |
| `stencilFuncSeparate` | **Yes** | Full. |
| `stencilMask` | **Yes** | Full. |
| `stencilMaskSeparate` | **Yes** | Full. |
| `stencilOp` | **Yes** | Full. |
| `stencilOpSeparate` | **Yes** | Full. |

## Buffers

| Method | Status | Notes |
|---|---|---|
| `bindBuffer` | **Yes** | Full. |
| `bufferData` | **Yes** | Both overloads: `(target, size, usage)` and `(target, typedArray, usage)`. |
| `bufferSubData` | **Yes** | TypedArray input only; plain ArrayBuffer not handled. |
| `createBuffer` | **Yes** | Full. |
| `deleteBuffer` | **Yes** | Full. |
| `getBufferParameter` | **Yes** | Full. |
| `isBuffer` | **Yes** | Full. |

## Framebuffers

| Method | Status | Notes |
|---|---|---|
| `bindFramebuffer` | **Yes** | Full. |
| `checkFramebufferStatus` | **Yes** | Full. |
| `createFramebuffer` | **Yes** | Full. |
| `deleteFramebuffer` | **Yes** | Full. |
| `framebufferRenderbuffer` | **Yes** | Full. |
| `framebufferTexture2D` | **Yes** | Full. |
| `getFramebufferAttachmentParameter` | **Yes** | Full. |
| `isFramebuffer` | **Yes** | Full. |

## Renderbuffers

| Method | Status | Notes |
|---|---|---|
| `bindRenderbuffer` | **Yes** | Full. |
| `createRenderbuffer` | **Yes** | Full. |
| `deleteRenderbuffer` | **Yes** | Full. |
| `getRenderbufferParameter` | **Yes** | Full. |
| `isRenderbuffer` | **Yes** | Full. |
| `renderbufferStorage` | **Yes** | Full. |

## Textures

| Method | Status | Notes |
|---|---|---|
| `bindTexture` | **Yes** | Full. |
| `compressedTexImage2D` | -- | |
| `compressedTexSubImage2D` | -- | |
| `copyTexImage2D` | **Yes** | Full. |
| `copyTexSubImage2D` | **Yes** | Full. |
| `createTexture` | **Yes** | Full. |
| `deleteTexture` | **Yes** | Full. |
| `generateMipmap` | **Yes** | Full. |
| `getTexParameter` | **Yes** | Full. |
| `isTexture` | **Yes** | Full. |
| `texImage2D` | **Yes\*** | 9-arg overload only (raw pixel data). HTMLImageElement / HTMLCanvasElement / ImageData source overloads not supported. |
| `texSubImage2D` | **Yes\*** | 9-arg overload only (raw pixel data). Image source overloads not supported. |
| `texParameterf` | **Yes** | Full. |
| `texParameteri` | **Yes** | Full. |

## Programs and Shaders

| Method | Status | Notes |
|---|---|---|
| `attachShader` | **Yes** | Full. |
| `bindAttribLocation` | **Yes** | Full. |
| `compileShader` | **Yes** | Full. |
| `createProgram` | **Yes** | Full. |
| `createShader` | **Yes** | Full. |
| `deleteProgram` | **Yes** | Full. |
| `deleteShader` | **Yes** | Full. |
| `detachShader` | **Yes** | Full. |
| `getAttachedShaders` | **Yes** | Full. Returns array of WebGLShader wrapper objects. |
| `getProgramParameter` | **Yes** | Returns boolean for LINK/VALIDATE/DELETE_STATUS, number otherwise. |
| `getProgramInfoLog` | **Yes** | Full. |
| `getShaderParameter` | **Yes** | Returns boolean for COMPILE/DELETE_STATUS, number otherwise. |
| `getShaderInfoLog` | **Yes** | Full. |
| `getShaderPrecisionFormat` | **Yes\*** | Stub; queries ANGLE but returns object with only `_id=0` instead of real rangeMin/rangeMax/precision. |
| `getShaderSource` | **Yes** | Full. |
| `isProgram` | **Yes** | Full. |
| `isShader` | **Yes** | Full. |
| `linkProgram` | **Yes** | Full. |
| `shaderSource` | **Yes** | Full. |
| `useProgram` | **Yes** | Full. |
| `validateProgram` | **Yes** | Full. |

## Uniforms and Attributes

| Method | Status | Notes |
|---|---|---|
| `disableVertexAttribArray` | **Yes** | Full. |
| `enableVertexAttribArray` | **Yes** | Full. |
| `getActiveAttrib` | **Yes\*** | Returns plain `{size, type, name}` object; `instanceof WebGLActiveInfo` is false. |
| `getActiveUniform` | **Yes\*** | Same as getActiveAttrib. |
| `getAttribLocation` | **Yes** | Full. |
| `getUniform` | **Yes** | Supports all uniform types (float/int/bool scalars, vectors, matrices, samplers). |
| `getUniformLocation` | **Yes** | Full. Returns null for loc < 0. |
| `getVertexAttrib` | **Yes** | Supports all pnames including CURRENT_VERTEX_ATTRIB and BUFFER_BINDING. |
| `getVertexAttribOffset` | **Yes** | Full. |
| `uniform1f` | **Yes** | Full. |
| `uniform1fv` | **Yes\*** | TypedArray only; plain JS Array not accepted. |
| `uniform1i` | **Yes** | Full. |
| `uniform1iv` | **Yes\*** | TypedArray only. |
| `uniform2f` | **Yes** | Full. |
| `uniform2fv` | **Yes\*** | TypedArray only. |
| `uniform2i` | **Yes** | Full. |
| `uniform2iv` | **Yes\*** | TypedArray only. |
| `uniform3f` | **Yes** | Full. |
| `uniform3fv` | **Yes\*** | TypedArray only. |
| `uniform3i` | **Yes** | Full. |
| `uniform3iv` | **Yes\*** | TypedArray only. |
| `uniform4f` | **Yes** | Full. |
| `uniform4fv` | **Yes\*** | TypedArray only. |
| `uniform4i` | **Yes** | Full. |
| `uniform4iv` | **Yes\*** | TypedArray only. |
| `uniformMatrix2fv` | **Yes\*** | TypedArray only. |
| `uniformMatrix3fv` | **Yes\*** | TypedArray only. |
| `uniformMatrix4fv` | **Yes\*** | TypedArray only. |
| `vertexAttrib1f` | **Yes** | Full. |
| `vertexAttrib1fv` | **Yes\*** | TypedArray only. |
| `vertexAttrib2f` | **Yes** | Full. |
| `vertexAttrib2fv` | **Yes\*** | TypedArray only. |
| `vertexAttrib3f` | **Yes** | Full. |
| `vertexAttrib3fv` | **Yes\*** | TypedArray only. |
| `vertexAttrib4f` | **Yes** | Full. |
| `vertexAttrib4fv` | **Yes\*** | TypedArray only. |
| `vertexAttribPointer` | **Yes** | Full. |

## Drawing

| Method | Status | Notes |
|---|---|---|
| `clear` | **Yes** | Full. |
| `drawArrays` | **Yes** | Full. |
| `drawElements` | **Yes** | Full. |
| `finish` | **Yes** | Full. |
| `flush` | **Yes** | Full. |

## Reading

| Method | Status | Notes |
|---|---|---|
| `readPixels` | **Yes** | Full. Writes directly into typed array backing buffer. |

---

## Known Limitations

1. **uniform*fv / uniformMatrix*fv** — Accept TypedArray input only. Plain
   JavaScript `Array` objects are not accepted (spec requires both).

2. **texImage2D / texSubImage2D** — Only the long-form 9-argument overload
   with raw pixel data is supported. Short-form overloads accepting
   HTMLImageElement, HTMLCanvasElement, HTMLVideoElement, or ImageData are not
   yet implemented.

3. **getShaderPrecisionFormat** — Stub implementation. Queries ANGLE but
   discards results; returned object lacks real rangeMin/rangeMax/precision.

4. **getActiveAttrib / getActiveUniform** — Return plain objects instead of
   WebGLActiveInfo instances.

5. **isContextLost** — Always returns false; no device-loss tracking.

6. **getSupportedExtensions / getExtension** — Hardcoded. Extension objects
   have no methods (e.g. WEBGL_lose_context returns `{}` with no
   `loseContext()`).

7. **getParameter** — Covers common pnames. Unknown pnames fall through to
   `glGetIntegerv` which may return wrong types for float/boolean/array params.

---

## Not Yet Implemented (2 methods)

### Texture Compressed
`compressedTexImage2D`, `compressedTexSubImage2D`

#### What's Needed

**C++ bridge work** (same 3 touch-points as every other method):

1. **`GL_compressedTexImage2D`** in `WebGLBridgeTextures.cpp`
   - Signature: `compressedTexImage2D(target, level, internalformat, width, height, border, data)`
   - Extract 6 numeric args + a typed array for `data`
   - Get the byte length from `JSObjectGetTypedArrayByteLength` (used as `imageSize`)
   - Get the data pointer from `JSObjectGetTypedArrayBytesPtr`
   - Call `glCompressedTexImage2D(target, level, internalformat, width, height, border, imageSize, ptr)`

2. **`GL_compressedTexSubImage2D`** in `WebGLBridgeTextures.cpp`
   - Signature: `compressedTexSubImage2D(target, level, xoffset, yoffset, width, height, format, data)`
   - Extract 7 numeric args + a typed array for `data`
   - Call `glCompressedTexSubImage2D(target, level, xoffset, yoffset, width, height, format, imageSize, ptr)`

3. Declarations in `WebGLBridgeInternal.h` + entries in `kWebGLFunctions[]`

**Extension / format considerations:**

The bridge functions themselves are straightforward typed-array-to-GL pass-throughs.
The harder part is that compressed textures are only useful if the client knows which
compressed formats are available. This requires:

- Advertising the correct WebGL extension(s) in `GL_getSupportedExtensions` /
  `GL_getExtension` (currently hardcoded). The relevant extensions are:
  - `WEBGL_compressed_texture_s3tc` (DXT1/DXT3/DXT5 — widely supported on desktop
    via ANGLE/D3D11)
  - `WEBGL_compressed_texture_etc` (ETC2 — may not be available on D3D11)
  - `WEBGL_compressed_texture_astc` (ASTC — typically mobile-only)
- Querying ANGLE at context creation to see which compressed formats are actually
  supported (`glGetIntegerv(GL_NUM_COMPRESSED_TEXTURE_FORMATS, ...)` and
  `glGetIntegerv(GL_COMPRESSED_TEXTURE_FORMATS, ...)`), then dynamically building
  the extension list instead of hardcoding it.
- Returning the correct format constants from `getParameter(COMPRESSED_TEXTURE_FORMATS)`
  in `GL_getParameter` (currently not handled — falls through to the `glGetIntegerv`
  default which only reads a single int, not an array).

**Minimal viable path:** If the goal is just S3TC (DXT) support on the D3D11 backend:
1. Add the two `glCompressedTex*` pass-through functions (~20 lines each)
2. Add `"WEBGL_compressed_texture_s3tc"` to the hardcoded extensions list
3. Add a `GL_COMPRESSED_TEXTURE_FORMATS` case to `GL_getParameter` that queries the
   full array from ANGLE and returns it as a JS array

---

## WebGL 2.0 (GLES3) Methods

Infrastructure: ANGLE is configured for OpenGL ES 3.0 (`EGL_OPENGL_ES3_BIT`,
`EGL_CONTEXT_CLIENT_VERSION 3`). All WebGL1 content continues to work unchanged.
The JS shim detects `'webgl2'` context requests and applies WebGL2 constants.

### Vertex Array Objects

| Method | Status | Notes |
|---|---|---|
| `createVertexArray` | **Yes** | `glGenVertexArrays`. Returns `WebGLVertexArrayObject`. |
| `deleteVertexArray` | **Yes** | `glDeleteVertexArrays`. |
| `bindVertexArray` | **Yes** | `glBindVertexArray`. Null unbinds (0). |
| `isVertexArray` | **Yes** | `glIsVertexArray`. |

### Instanced Drawing

| Method | Status | Notes |
|---|---|---|
| `drawArraysInstanced` | **Yes** | Full + flush + readback. |
| `drawElementsInstanced` | **Yes** | Full + flush + readback. |
| `drawRangeElements` | **Yes** | Full + flush + readback. |
| `vertexAttribDivisor` | **Yes** | Full. |

### Uniform Buffer Objects

| Method | Status | Notes |
|---|---|---|
| `bindBufferBase` | **Yes** | Full. |
| `bindBufferRange` | **Yes** | Full. |
| `uniformBlockBinding` | **Yes** | Full. |
| `getUniformBlockIndex` | **Yes** | Returns `GL_INVALID_INDEX` on failure. |
| `getActiveUniformBlockName` | **Yes** | Full. |
| `getActiveUniformBlockParameter` | **Yes** | Switches on pname for correct return type. |
| `getUniformIndices` | **Yes** | JS string array → GLuint array. |
| `getActiveUniforms` | **Yes** | Full. |

### Framebuffer Enhancements

| Method | Status | Notes |
|---|---|---|
| `drawBuffers` | **Yes** | JS array → `glDrawBuffers`. |
| `readBuffer` | **Yes** | Full. |
| `blitFramebuffer` | **Yes** | Full (10 args). |
| `framebufferTextureLayer` | **Yes** | Full. |
| `renderbufferStorageMultisample` | **Yes** | Full. |
| `invalidateFramebuffer` | **Yes** | JS array → `glInvalidateFramebuffer`. |
| `invalidateSubFramebuffer` | **Yes** | JS array + rect args. |

### Transform Feedback

| Method | Status | Notes |
|---|---|---|
| `createTransformFeedback` | **Yes** | `glGenTransformFeedbacks`. Returns `WebGLTransformFeedback`. |
| `deleteTransformFeedback` | **Yes** | Full. |
| `bindTransformFeedback` | **Yes** | Full. Null unbinds. |
| `isTransformFeedback` | **Yes** | Full. |
| `beginTransformFeedback` | **Yes** | Full. |
| `endTransformFeedback` | **Yes** | Full. |
| `pauseTransformFeedback` | **Yes** | Full. |
| `resumeTransformFeedback` | **Yes** | Full. |
| `transformFeedbackVaryings` | **Yes** | JS string array → `glTransformFeedbackVaryings`. |
| `getTransformFeedbackVarying` | **Yes** | Returns WebGLActiveInfo-like object. |

### Uint Uniforms

| Method | Status | Notes |
|---|---|---|
| `uniform1ui` – `uniform4ui` | **Yes** | Full. |
| `uniform1uiv` – `uniform4uiv` | **Yes** | TypedArray input only. |

### Non-Square Matrix Uniforms

| Method | Status | Notes |
|---|---|---|
| `uniformMatrix2x3fv`, `uniformMatrix3x2fv` | **Yes** | TypedArray input only. |
| `uniformMatrix2x4fv`, `uniformMatrix4x2fv` | **Yes** | TypedArray input only. |
| `uniformMatrix3x4fv`, `uniformMatrix4x3fv` | **Yes** | TypedArray input only. |

### Integer Vertex Attribs

| Method | Status | Notes |
|---|---|---|
| `vertexAttribIPointer` | **Yes** | Full. |
| `vertexAttribI4i`, `vertexAttribI4ui` | **Yes** | Full. |
| `vertexAttribI4iv`, `vertexAttribI4uiv` | **Yes** | TypedArray input. |

### Clear Buffer

| Method | Status | Notes |
|---|---|---|
| `clearBufferiv` | **Yes** | TypedArray input. |
| `clearBufferuiv` | **Yes** | TypedArray input. |
| `clearBufferfv` | **Yes** | TypedArray input. |
| `clearBufferfi` | **Yes** | Full. |

### 3D Textures & Storage

| Method | Status | Notes |
|---|---|---|
| `texStorage2D` | **Yes** | Full. |
| `texStorage3D` | **Yes** | Full. |
| `texImage3D` | **Yes** | Null data supported. TypedArray input only. |
| `texSubImage3D` | **Yes** | TypedArray input only. |
| `copyTexSubImage3D` | **Yes** | Full. |

### Samplers

| Method | Status | Notes |
|---|---|---|
| `createSampler` | **Yes** | Returns `WebGLSampler`. |
| `deleteSampler` | **Yes** | Full. |
| `bindSampler` | **Yes** | Null unbinds. |
| `isSampler` | **Yes** | Full. |
| `samplerParameteri` | **Yes** | Full. |
| `samplerParameterf` | **Yes** | Full. |
| `getSamplerParameter` | **Yes** | Switches on pname for float vs int return. |

### Queries

| Method | Status | Notes |
|---|---|---|
| `createQuery` | **Yes** | Returns `WebGLQuery`. |
| `deleteQuery` | **Yes** | Full. |
| `isQuery` | **Yes** | Full. |
| `beginQuery` | **Yes** | Full. |
| `endQuery` | **Yes** | Full. |
| `getQuery` | **Yes** | `CURRENT_QUERY` returns WebGLQuery object. |
| `getQueryParameter` | **Yes** | `QUERY_RESULT_AVAILABLE` returns boolean. |

### Sync Objects

| Method | Status | Notes |
|---|---|---|
| `fenceSync` | **Yes** | Returns `WebGLSync`. GLsync stored as uintptr_t in `_id`. |
| `isSync` | **Yes** | Full. |
| `deleteSync` | **Yes** | Full. |
| `clientWaitSync` | **Yes** | Returns GLenum status. |
| `waitSync` | **Yes** | Full. |
| `getSyncParameter` | **Yes** | Full. |

### Buffer Operations

| Method | Status | Notes |
|---|---|---|
| `copyBufferSubData` | **Yes** | Full. |
| `getBufferSubData` | **Yes** | Emulated via `glMapBufferRange(GL_MAP_READ_BIT)` + memcpy. |

### Misc Queries

| Method | Status | Notes |
|---|---|---|
| `getFragDataLocation` | **Yes** | Full. |
| `getIndexedParameter` | **Yes** | Buffer bindings return WebGLBuffer objects. |

### WebGL2 Extensions

| Extension | Status | Notes |
|---|---|---|
| `EXT_color_buffer_float` | **Yes** | Returned by `getExtension`. |
| `EXT_color_buffer_half_float` | **Yes** | Returned by `getExtension`. |
| `OES_texture_float_linear` | **Yes** | Returned by `getExtension`. |
| `EXT_float_blend` | **Yes** | Returned by `getExtension`. |
| `EXT_texture_filter_anisotropic` | **Yes** | Returned by `getExtension`. |

---

## WebGL 2.0 Known Limitations

1. **GLsync pointer storage** — `fenceSync` stores the `GLsync` pointer as a
   32-bit `GLuint` via `uintptr_t` truncation. Works on current 64-bit ANGLE
   builds where sync handles fit in 32 bits, but may need widening if ANGLE
   changes its handle format.

2. **texImage3D / texSubImage3D** — TypedArray input only. No HTMLImageElement
   or ImageData overloads.

3. **getBufferSubData** — Emulated via `glMapBufferRange` since GLES3 lacks
   `glGetBufferSubData`. Performance may be lower than native implementations.

4. **getParameter** — New WebGL2 pnames fall through to the existing
   `glGetIntegerv` default case, which works for most integer params but may
   return wrong types for array or boolean params not explicitly handled.
