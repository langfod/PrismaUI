# PrismaUI WebGL API Coverage

WebGL 1.0 implementation status for PrismaUI's ANGLE-based WebGL bridge.

**Coverage: 87 / 131 methods (~66%)**

The implemented subset targets THREE.js and common WebGL tutorials. Functions
marked "full" are direct pass-throughs to ANGLE's GLES2 backend with no known
limitations.

---

## Canvas / Context

| Method | Status | Notes |
|---|---|---|
| `getContextAttributes` | -- | |
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
| `hint` | -- | |
| `isEnabled` | -- | |
| `lineWidth` | **Yes** | Full (many drivers clamp to 1.0). |
| `pixelStorei` | **Yes** | Full. |
| `polygonOffset` | **Yes** | Full. |
| `sampleCoverage` | -- | |
| `stencilFunc` | **Yes** | Full. |
| `stencilFuncSeparate` | -- | |
| `stencilMask` | **Yes** | Full. |
| `stencilMaskSeparate` | -- | |
| `stencilOp` | **Yes** | Full. |
| `stencilOpSeparate` | -- | |

## Buffers

| Method | Status | Notes |
|---|---|---|
| `bindBuffer` | **Yes** | Full. |
| `bufferData` | **Yes** | Both overloads: `(target, size, usage)` and `(target, typedArray, usage)`. |
| `bufferSubData` | **Yes** | TypedArray input only; plain ArrayBuffer not handled. |
| `createBuffer` | **Yes** | Full. |
| `deleteBuffer` | **Yes** | Full. |
| `getBufferParameter` | -- | |
| `isBuffer` | -- | |

## Framebuffers

| Method | Status | Notes |
|---|---|---|
| `bindFramebuffer` | **Yes** | Full. |
| `checkFramebufferStatus` | **Yes** | Full. |
| `createFramebuffer` | **Yes** | Full. |
| `deleteFramebuffer` | **Yes** | Full. |
| `framebufferRenderbuffer` | **Yes** | Full. |
| `framebufferTexture2D` | **Yes** | Full. |
| `getFramebufferAttachmentParameter` | -- | |
| `isFramebuffer` | -- | |

## Renderbuffers

| Method | Status | Notes |
|---|---|---|
| `bindRenderbuffer` | **Yes** | Full. |
| `createRenderbuffer` | **Yes** | Full. |
| `deleteRenderbuffer` | **Yes** | Full. |
| `getRenderbufferParameter` | -- | |
| `isRenderbuffer` | -- | |
| `renderbufferStorage` | **Yes** | Full. |

## Textures

| Method | Status | Notes |
|---|---|---|
| `bindTexture` | **Yes** | Full. |
| `compressedTexImage2D` | -- | |
| `compressedTexSubImage2D` | -- | |
| `copyTexImage2D` | -- | |
| `copyTexSubImage2D` | -- | |
| `createTexture` | **Yes** | Full. |
| `deleteTexture` | **Yes** | Full. |
| `generateMipmap` | **Yes** | Full. |
| `getTexParameter` | -- | |
| `isTexture` | -- | |
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
| `detachShader` | -- | |
| `getAttachedShaders` | -- | |
| `getProgramParameter` | **Yes** | Returns boolean for LINK/VALIDATE/DELETE_STATUS, number otherwise. |
| `getProgramInfoLog` | **Yes** | Full. |
| `getShaderParameter` | **Yes** | Returns boolean for COMPILE/DELETE_STATUS, number otherwise. |
| `getShaderInfoLog` | **Yes** | Full. |
| `getShaderPrecisionFormat` | **Yes\*** | Stub; queries ANGLE but returns object with only `_id=0` instead of real rangeMin/rangeMax/precision. |
| `getShaderSource` | -- | |
| `isProgram` | -- | |
| `isShader` | -- | |
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
| `getUniform` | -- | |
| `getUniformLocation` | **Yes** | Full. Returns null for loc < 0. |
| `getVertexAttrib` | -- | |
| `getVertexAttribOffset` | -- | |
| `uniform1f` | **Yes** | Full. |
| `uniform1fv` | **Yes\*** | TypedArray only; plain JS Array not accepted. |
| `uniform1i` | **Yes** | Full. |
| `uniform1iv` | **Yes\*** | TypedArray only. |
| `uniform2f` | **Yes** | Full. |
| `uniform2fv` | **Yes\*** | TypedArray only. |
| `uniform2i` | -- | |
| `uniform2iv` | -- | |
| `uniform3f` | **Yes** | Full. |
| `uniform3fv` | **Yes\*** | TypedArray only. |
| `uniform3i` | -- | |
| `uniform3iv` | -- | |
| `uniform4f` | **Yes** | Full. |
| `uniform4fv` | **Yes\*** | TypedArray only. |
| `uniform4i` | -- | |
| `uniform4iv` | -- | |
| `uniformMatrix2fv` | **Yes\*** | TypedArray only. |
| `uniformMatrix3fv` | **Yes\*** | TypedArray only. |
| `uniformMatrix4fv` | **Yes\*** | TypedArray only. |
| `vertexAttrib1f` | -- | |
| `vertexAttrib1fv` | -- | |
| `vertexAttrib2f` | -- | |
| `vertexAttrib2fv` | -- | |
| `vertexAttrib3f` | -- | |
| `vertexAttrib3fv` | -- | |
| `vertexAttrib4f` | -- | |
| `vertexAttrib4fv` | -- | |
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
| `readPixels` | -- | |

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

## Not Yet Implemented (44 methods)

### Introspection / Query
`getContextAttributes`, `hint`, `isEnabled`, `getBufferParameter`, `isBuffer`,
`getFramebufferAttachmentParameter`, `isFramebuffer`,
`getRenderbufferParameter`, `isRenderbuffer`, `getTexParameter`, `isTexture`,
`getAttachedShaders`, `getShaderSource`, `isProgram`, `isShader`, `getUniform`,
`getVertexAttrib`, `getVertexAttribOffset`, `readPixels`

### Stencil (separate face)
`stencilFuncSeparate`, `stencilMaskSeparate`, `stencilOpSeparate`

### Misc State
`sampleCoverage`

### Texture Copy / Compressed
`compressedTexImage2D`, `compressedTexSubImage2D`, `copyTexImage2D`,
`copyTexSubImage2D`

### Shader
`detachShader`

### Integer Uniforms (2/3/4 component)
`uniform2i`, `uniform2iv`, `uniform3i`, `uniform3iv`, `uniform4i`, `uniform4iv`

### Vertex Attrib Constants
`vertexAttrib1f`, `vertexAttrib1fv`, `vertexAttrib2f`, `vertexAttrib2fv`,
`vertexAttrib3f`, `vertexAttrib3fv`, `vertexAttrib4f`, `vertexAttrib4fv`
