(function() {
    'use strict';

    // Guard: don't re-inject if already loaded in this JS context
    if (window.__prismaWebGLShimLoaded) return;
    window.__prismaWebGLShimLoaded = true;

    // =========================================================================
    // WebGL wrapper classes for GL object handles
    // =========================================================================
    class WebGLBuffer       { constructor(id) { this._id = id; } }
    class WebGLTexture      { constructor(id) { this._id = id; } }
    class WebGLShader       { constructor(id) { this._id = id; } }
    class WebGLProgram      { constructor(id) { this._id = id; } }
    class WebGLFramebuffer  { constructor(id) { this._id = id; } }
    class WebGLRenderbuffer { constructor(id) { this._id = id; } }
    class WebGLUniformLocation { constructor(id) { this._id = id; } }
    class WebGLActiveInfo {
        constructor(size, type, name) {
            this.size = size;
            this.type = type;
            this.name = name;
        }
    }
    class WebGLShaderPrecisionFormat {
        constructor(rangeMin, rangeMax, precision) {
            this.rangeMin = rangeMin;
            this.rangeMax = rangeMax;
            this.precision = precision;
        }
    }

    // WebGL2 wrapper classes
    class WebGLVertexArrayObject  { constructor(id) { this._id = id; } }
    class WebGLSampler            { constructor(id) { this._id = id; } }
    class WebGLSync               { constructor(id) { this._id = id; } }
    class WebGLTransformFeedback  { constructor(id) { this._id = id; } }
    class WebGLQuery              { constructor(id) { this._id = id; } }

    // Expose wrapper classes globally so C++ bridge can use them
    window.WebGLBuffer = WebGLBuffer;
    window.WebGLTexture = WebGLTexture;
    window.WebGLShader = WebGLShader;
    window.WebGLProgram = WebGLProgram;
    window.WebGLFramebuffer = WebGLFramebuffer;
    window.WebGLRenderbuffer = WebGLRenderbuffer;
    window.WebGLUniformLocation = WebGLUniformLocation;
    window.WebGLActiveInfo = WebGLActiveInfo;
    window.WebGLShaderPrecisionFormat = WebGLShaderPrecisionFormat;
    window.WebGLVertexArrayObject = WebGLVertexArrayObject;
    window.WebGLSampler = WebGLSampler;
    window.WebGLSync = WebGLSync;
    window.WebGLTransformFeedback = WebGLTransformFeedback;
    window.WebGLQuery = WebGLQuery;

    // =========================================================================
    // Override HTMLCanvasElement.prototype.getContext
    // =========================================================================
    const _origGetContext = HTMLCanvasElement.prototype.getContext;

    function __prismaIsHidden(el) {
        if (!el || !el.ownerDocument || !el.ownerDocument.defaultView) return false;
        var win = el.ownerDocument.defaultView;
        if (!win.getComputedStyle) return false;
        // Walk the DOM ancestor chain - a parent with display:none hides all descendants
        var check = el;
        while (check && check.nodeType === 1) {
            var st = win.getComputedStyle(check);
            if (st && (st.display === 'none' || st.visibility === 'hidden' || st.opacity === '0')) {
                return true;
            }
            check = check.parentElement;
        }
        return false;
    }

    function __prismaComputeCanvasInfo(canvas) {
        var rect = { left: 0, top: 0, width: 0, height: 0 };
        if (typeof canvas.getBoundingClientRect === 'function') {
            rect = canvas.getBoundingClientRect();
        }

        // Buffer size (ANGLE pbuffer dimensions) — always canvas.width/height
        var w = canvas.width || 300;
        var h = canvas.height || 150;

        // CSS display size — what the user sees on screen.
        // Priority: getBoundingClientRect > clientWidth/Height > inline style > canvas.width/height
        var dw = rect.width || canvas.clientWidth || parseFloat(canvas.style.width) || w;
        var dh = rect.height || canvas.clientHeight || parseFloat(canvas.style.height) || h;

        var visible = true;

        // In Ultralight iframes, getBoundingClientRect() may return all zeros
        // when CSS layout hasn't resolved (e.g. percentage/flex sizing).
        var hasCanvasAttribSize = (canvas.width > 0 && canvas.height > 0);
        var rectResolved = (rect.width > 0 && rect.height > 0);

        // Start with the canvas position within its document.
        var x = rect.left || 0;
        var y = rect.top || 0;

        // Fallback: when getBoundingClientRect returns zeros, walk the
        // offsetParent chain to compute position within the iframe document.
        if (!rectResolved && canvas.offsetParent !== undefined) {
            var el = canvas;
            var ox = 0, oy = 0;
            while (el) {
                ox += (el.offsetLeft || 0);
                oy += (el.offsetTop || 0);
                el = el.offsetParent;
            }
            x = ox;
            y = oy;
        }

        // Last-resort fallback: if position is still (0,0), try parsing
        // inline style left/top (set by JS layoutCanvas when CSS layout
        // doesn't produce offset values in Ultralight iframes).
        if (x === 0 && y === 0) {
            var sl = parseFloat(canvas.style.left);
            var st = parseFloat(canvas.style.top);
            if (isFinite(sl)) x = sl;
            if (isFinite(st)) y = st;
        }

        // Debug: log all sources so we can diagnose position issues
        if (!canvas.__prismaCanvasInfoLogged) {
            canvas.__prismaCanvasInfoLogged = true;
            console.log('[canvasInfo] rect=' + rect.left + ',' + rect.top + ',' + rect.width + ',' + rect.height +
                        ' offset=' + canvas.offsetLeft + ',' + canvas.offsetTop +
                        ' style=' + canvas.style.left + ',' + canvas.style.top + ',' + canvas.style.width + ',' + canvas.style.height +
                        ' client=' + canvas.clientWidth + 'x' + canvas.clientHeight +
                        ' attrib=' + canvas.width + 'x' + canvas.height +
                        ' => x=' + x + ' y=' + y + ' dw=' + dw + ' dh=' + dh);
        }

        // Only run isHidden on the canvas if the CSS rect resolved.
        if (rectResolved && __prismaIsHidden(canvas)) {
            visible = false;
        }

        // Walk the iframe chain to accumulate parent frame offsets.
        var win = canvas.ownerDocument && canvas.ownerDocument.defaultView;
        var walkedFrames = false;
        while (win && win.frameElement) {
            walkedFrames = true;
            var fe = win.frameElement;
            if (__prismaIsHidden(fe)) {
                visible = false;
            }
            if (typeof fe.getBoundingClientRect === 'function') {
                var fr = fe.getBoundingClientRect();
                x += (fr.left || 0) + (fe.clientLeft || 0);
                y += (fr.top || 0) + (fe.clientTop || 0);
            }
            win = win.parent;
        }

        // Fallback: if we couldn't walk any frames (cross-origin), use the
        // pre-computed offset from the parent's injection script.
        if (!walkedFrames && win) {
            if (typeof win.__prismaFrameOffsetX === 'number') {
                x += win.__prismaFrameOffsetX;
            }
            if (typeof win.__prismaFrameOffsetY === 'number') {
                y += win.__prismaFrameOffsetY;
            }
        }

        // Only treat as invisible when there is genuinely no size.
        if (w <= 0 || h <= 0) {
            visible = false;
        }
        if (!hasCanvasAttribSize && !rectResolved) {
            visible = false;
        }

        return { x: x, y: y, w: w, h: h, dw: dw, dh: dh, visible: visible };
    }

    function __prismaUpdateAllWebGL() {
        if (typeof __prismaUpdateWebGLContext !== 'function') return;
        if (!window.__prismaWebGLContexts) return;
        for (var i = 0; i < window.__prismaWebGLContexts.length; i++) {
            var ctx = window.__prismaWebGLContexts[i];
            if (!ctx || !ctx.canvas) continue;
            var info = __prismaComputeCanvasInfo(ctx.canvas);
            __prismaUpdateWebGLContext(ctx, info.x, info.y, info.w, info.h, info.visible, info.dw, info.dh);
        }
    }

    HTMLCanvasElement.prototype.getContext = function(type, attrs) {
        if (type === 'webgl' || type === 'experimental-webgl' || type === 'webgl2') {

            // Return cached context if already created for this canvas
            if (this.__prismaWebGLContext) {
                return this.__prismaWebGLContext;
            }

            // __prismaCreateWebGLContext is a native C++ function bound by WebGLBridge
            if (typeof __prismaCreateWebGLContext === 'function') {
                const w = this.width || 300;
                const h = this.height || 150;
                var info = __prismaComputeCanvasInfo(this);
                const ctx = __prismaCreateWebGLContext(w, h, info.x, info.y);
                if (ctx) {
                    // Store reference to canvas on the context
                    ctx.canvas = this;
                    ctx._canvasElement = this;
                    this.__prismaWebGLContext = ctx;

                    if (!window.__prismaWebGLContexts) {
                        window.__prismaWebGLContexts = [];
                    }
                    window.__prismaWebGLContexts.push(ctx);

                    // Set up drawingBufferWidth/Height
                    Object.defineProperty(ctx, 'drawingBufferWidth', {
                        get: function() { return this.canvas.width || 300; }
                    });
                    Object.defineProperty(ctx, 'drawingBufferHeight', {
                        get: function() { return this.canvas.height || 150; }
                    });

                    // Copy all WebGL constants onto the context instance
                    // so that gl.ARRAY_BUFFER, gl.FLOAT, etc. work as expected
                    if (window.WebGLRenderingContext) {
                        var GL = window.WebGLRenderingContext;
                        for (var k in GL) {
                            if (GL.hasOwnProperty(k) && typeof GL[k] === 'number') {
                                ctx[k] = GL[k];
                            }
                        }
                    }
                    // For webgl2 contexts, also copy WebGL2-specific constants
                    if (type === 'webgl2' && window.WebGL2RenderingContext) {
                        var GL2 = window.WebGL2RenderingContext;
                        for (var k in GL2) {
                            if (GL2.hasOwnProperty(k) && typeof GL2[k] === 'number') {
                                ctx[k] = GL2[k];
                            }
                        }
                    }

                    // Wrap getExtension to log unsupported extension requests
                    // and provide JS polyfills for extensions that can be emulated.
                    var _origGetExtension = ctx.getExtension.bind(ctx);
                    ctx.getExtension = function(name) {
                        var ext = _origGetExtension(name);
                        if (ext) return ext;

                        // WEBGL_multi_draw polyfill: batch draw calls via loop.
                        // The native ANGLE build may not expose GL_ANGLE_multi_draw,
                        // but we can emulate it with individual drawArrays/drawElements.
                        if (name === 'WEBGL_multi_draw') {
                            var _da = ctx.drawArrays.bind(ctx);
                            var _de = ctx.drawElements.bind(ctx);
                            return {
                                multiDrawArraysWEBGL: function(mode, firstsList, firstsOffset,
                                                               countsList, countsOffset, drawcount) {
                                    for (var i = 0; i < drawcount; i++) {
                                        _da(mode, firstsList[firstsOffset + i], countsList[countsOffset + i]);
                                    }
                                },
                                multiDrawElementsWEBGL: function(mode, countsList, countsOffset,
                                                                  type, offsetsList, offsetsOffset, drawcount) {
                                    for (var i = 0; i < drawcount; i++) {
                                        _de(mode, countsList[countsOffset + i], type, offsetsList[offsetsOffset + i]);
                                    }
                                },
                                multiDrawArraysInstancedWEBGL: function(mode, firstsList, firstsOffset,
                                                                        countsList, countsOffset,
                                                                        instanceCountsList, instanceCountsOffset,
                                                                        drawcount) {
                                    var _dai = ctx.drawArraysInstanced.bind(ctx);
                                    for (var i = 0; i < drawcount; i++) {
                                        _dai(mode, firstsList[firstsOffset + i],
                                             countsList[countsOffset + i],
                                             instanceCountsList[instanceCountsOffset + i]);
                                    }
                                },
                                multiDrawElementsInstancedWEBGL: function(mode, countsList, countsOffset,
                                                                          type, offsetsList, offsetsOffset,
                                                                          instanceCountsList, instanceCountsOffset,
                                                                          drawcount) {
                                    var _dei = ctx.drawElementsInstanced.bind(ctx);
                                    for (var i = 0; i < drawcount; i++) {
                                        _dei(mode, countsList[countsOffset + i], type,
                                             offsetsList[offsetsOffset + i],
                                             instanceCountsList[instanceCountsOffset + i]);
                                    }
                                }
                            };
                        }

                        console.info('[WebGL] Extension not available: ' + name);
                        return ext;
                    };

                    // Wrap getSupportedExtensions so JS-polyfilled extensions
                    // (like WEBGL_multi_draw) appear in the list.
                    var _origGetSupportedExtensions = ctx.getSupportedExtensions.bind(ctx);
                    ctx.getSupportedExtensions = function() {
                        var list = _origGetSupportedExtensions() || [];
                        if (list.indexOf('WEBGL_multi_draw') === -1) {
                            list.push('WEBGL_multi_draw');
                        }
                        return list;
                    };

                    // -------------------------------------------------------
                    // Wrap texImage2D / texSubImage2D to handle HTMLImageElement,
                    // HTMLCanvasElement, and ImageData sources (6-arg and 7-arg forms)
                    // -------------------------------------------------------
                    var _origTexImage2D = ctx.texImage2D.bind(ctx);
                    var _origTexSubImage2D = ctx.texSubImage2D.bind(ctx);

                    function __prismaIsImageSource(obj) {
                        return (obj instanceof HTMLImageElement) ||
                               (obj instanceof HTMLCanvasElement) ||
                               (typeof ImageBitmap !== 'undefined' && obj instanceof ImageBitmap) ||
                               (typeof ImageData !== 'undefined' && obj instanceof ImageData);
                    }

                    function __prismaExtractPixels(source) {
                        var w, h, cvs, c2d;
                        if (typeof ImageData !== 'undefined' && source instanceof ImageData) {
                            return { data: new Uint8Array(source.data.buffer), width: source.width, height: source.height };
                        }
                        if (source instanceof HTMLCanvasElement) {
                            cvs = source;
                            w = cvs.width;
                            h = cvs.height;
                        } else {
                            // HTMLImageElement or ImageBitmap
                            w = source.naturalWidth || source.width;
                            h = source.naturalHeight || source.height;
                            if (!w || !h) {
                                console.warn('[WebGL] texImage2D: image source has 0 dimensions');
                                return null;
                            }
                            cvs = document.createElement('canvas');
                            cvs.width = w;
                            cvs.height = h;
                        }
                        c2d = cvs.getContext('2d');
                        if (!c2d) {
                            console.warn('[WebGL] texImage2D: failed to get 2d context for pixel extraction');
                            return null;
                        }
                        if (source !== cvs) {
                            c2d.drawImage(source, 0, 0);
                        }
                        var imageData = c2d.getImageData(0, 0, w, h);
                        return { data: new Uint8Array(imageData.data.buffer), width: w, height: h };
                    }

                    ctx.texImage2D = function() {
                        var args = arguments;
                        // 6-arg form: (target, level, internalformat, format, type, source)
                        if (args.length === 6 && __prismaIsImageSource(args[5])) {
                            var pixels = __prismaExtractPixels(args[5]);
                            if (pixels) {
                                return _origTexImage2D(args[0], args[1], args[2],
                                    pixels.width, pixels.height, 0, args[3], args[4], pixels.data);
                            }
                            return;
                        }
                        // 9-arg form where arg[8] is an image source instead of typed array
                        if (args.length >= 9 && args[8] && __prismaIsImageSource(args[8])) {
                            var pixels = __prismaExtractPixels(args[8]);
                            if (pixels) {
                                return _origTexImage2D(args[0], args[1], args[2],
                                    args[3], args[4], args[5], args[6], args[7], pixels.data);
                            }
                            return;
                        }
                        return _origTexImage2D.apply(null, args);
                    };

                    ctx.texSubImage2D = function() {
                        var args = arguments;
                        // 7-arg form: (target, level, xoffset, yoffset, format, type, source)
                        if (args.length === 7 && __prismaIsImageSource(args[6])) {
                            var pixels = __prismaExtractPixels(args[6]);
                            if (pixels) {
                                return _origTexSubImage2D(args[0], args[1], args[2], args[3],
                                    pixels.width, pixels.height, args[4], args[5], pixels.data);
                            }
                            return;
                        }
                        // 9-arg form where arg[8] is an image source
                        if (args.length >= 9 && args[8] && __prismaIsImageSource(args[8])) {
                            var pixels = __prismaExtractPixels(args[8]);
                            if (pixels) {
                                return _origTexSubImage2D(args[0], args[1], args[2], args[3],
                                    args[4], args[5], args[6], args[7], pixels.data);
                            }
                            return;
                        }
                        return _origTexSubImage2D.apply(null, args);
                    };

                    // Wrap context in Proxy to log calls to unimplemented methods.
                    // IMPORTANT: Native JSC functions use JSObjectGetPrivate(thisObject)
                    // to retrieve the ANGLE context.  If `this` is the Proxy instead of
                    // the raw target, JSObjectGetPrivate returns nullptr ⇒ null results.
                    // So we bind every native function to the real target object.
                    const proxied = new Proxy(ctx, {
                        get(target, prop, receiver) {
                            if (prop in target) {
                                var val = target[prop];
                                if (typeof val === 'function') {
                                    return val.bind(target);
                                }
                                return val;
                            }
                            if (typeof prop === 'string') {
                                return function() {
                                    console.warn('[WebGL] ' + prop + ' is not implemented');
                                    return undefined;
                                };
                            }
                            return undefined;
                        }
                    });
                    this.__prismaWebGLContext = proxied;
                    // Keep the RAW native ctx in __prismaWebGLContexts (not the proxy)
                    // because __prismaUpdateWebGLContext is a C++ function that calls
                    // JSObjectGetPrivate() — which returns nullptr for a Proxy object.

                    // Kick an initial update so position/visibility are synced
                    __prismaUpdateAllWebGL();

                    return proxied;
                }
            }

            // If native bridge not available, return null (WebGL not supported)
            return null;
        }

        // Fall through to original for '2d' and other context types
        if (type !== '2d' && type !== 'bitmaprenderer') {
            console.warn('[WebGL] Unsupported context type: ' + type);
        }
        return _origGetContext.call(this, type, attrs);
    };

    // Also override OffscreenCanvas if it exists
    if (typeof OffscreenCanvas !== 'undefined') {
        const _origOffscreenGetContext = OffscreenCanvas.prototype.getContext;
        OffscreenCanvas.prototype.getContext = function(type, attrs) {
            if (type === 'webgl' || type === 'experimental-webgl' || type === 'webgl2') {
                console.warn('[WebGL] OffscreenCanvas WebGL not supported (requested: ' + type + ')');
                return null;
            }
            return _origOffscreenGetContext.call(this, type, attrs);
        };
    }

    // Wrap requestAnimationFrame to keep canvas position/visibility updated
    if (!window.__prismaWrappedRAF && typeof window.requestAnimationFrame === 'function') {
        window.__prismaWrappedRAF = true;
        const _origRAF = window.requestAnimationFrame;
        window.requestAnimationFrame = function(cb) {
            return _origRAF.call(this, function(ts) {
                try { __prismaUpdateAllWebGL(); } catch (e) {}
                return cb(ts);
            });
        };
    }

    if (!window.__prismaWebGLUpdateTimer) {
        window.__prismaWebGLUpdateTimer = setInterval(function() {
            try { __prismaUpdateAllWebGL(); } catch (e) {}
        }, 200);
    }

    // =========================================================================
    // WebGL constants
    // Defined on window.WebGLRenderingContext for spec compatibility checks,
    // and copied onto each context instance in the getContext override above.
    // =========================================================================
    window.WebGLRenderingContext = function() {};

    var GL = window.WebGLRenderingContext;

    // Data types
    GL.BYTE = 0x1400; GL.UNSIGNED_BYTE = 0x1401;
    GL.SHORT = 0x1402; GL.UNSIGNED_SHORT = 0x1403;
    GL.INT = 0x1404; GL.UNSIGNED_INT = 0x1405;
    GL.FLOAT = 0x1406;

    // Primitives
    GL.POINTS = 0x0000; GL.LINES = 0x0001; GL.LINE_LOOP = 0x0002;
    GL.LINE_STRIP = 0x0003; GL.TRIANGLES = 0x0004;
    GL.TRIANGLE_STRIP = 0x0005; GL.TRIANGLE_FAN = 0x0006;

    // Blending
    GL.ZERO = 0; GL.ONE = 1;
    GL.SRC_COLOR = 0x0300; GL.ONE_MINUS_SRC_COLOR = 0x0301;
    GL.SRC_ALPHA = 0x0302; GL.ONE_MINUS_SRC_ALPHA = 0x0303;
    GL.DST_ALPHA = 0x0304; GL.ONE_MINUS_DST_ALPHA = 0x0305;
    GL.DST_COLOR = 0x0306; GL.ONE_MINUS_DST_COLOR = 0x0307;
    GL.SRC_ALPHA_SATURATE = 0x0308;
    GL.FUNC_ADD = 0x8006;
    GL.FUNC_SUBTRACT = 0x800A; GL.FUNC_REVERSE_SUBTRACT = 0x800B;
    GL.BLEND_EQUATION = 0x8009; GL.BLEND_EQUATION_RGB = 0x8009;
    GL.BLEND_EQUATION_ALPHA = 0x883D;
    GL.BLEND_DST_RGB = 0x80C8; GL.BLEND_SRC_RGB = 0x80C9;
    GL.BLEND_DST_ALPHA = 0x80CA; GL.BLEND_SRC_ALPHA = 0x80CB;
    GL.CONSTANT_COLOR = 0x8001; GL.ONE_MINUS_CONSTANT_COLOR = 0x8002;
    GL.CONSTANT_ALPHA = 0x8003; GL.ONE_MINUS_CONSTANT_ALPHA = 0x8004;
    GL.BLEND_COLOR = 0x8005;

    // Buffer objects
    GL.ARRAY_BUFFER = 0x8892; GL.ELEMENT_ARRAY_BUFFER = 0x8893;
    GL.ARRAY_BUFFER_BINDING = 0x8894; GL.ELEMENT_ARRAY_BUFFER_BINDING = 0x8895;
    GL.STREAM_DRAW = 0x88E0; GL.STATIC_DRAW = 0x88E4; GL.DYNAMIC_DRAW = 0x88E8;
    GL.BUFFER_SIZE = 0x8764; GL.BUFFER_USAGE = 0x8765;

    // Culling
    GL.FRONT = 0x0404; GL.BACK = 0x0405; GL.FRONT_AND_BACK = 0x0408;
    GL.CW = 0x0900; GL.CCW = 0x0901;
    GL.CULL_FACE = 0x0B44;

    // Depth
    GL.DEPTH_TEST = 0x0B71;
    GL.NEVER = 0x0200; GL.LESS = 0x0201; GL.EQUAL = 0x0202;
    GL.LEQUAL = 0x0203; GL.GREATER = 0x0204; GL.NOTEQUAL = 0x0205;
    GL.GEQUAL = 0x0206; GL.ALWAYS = 0x0207;
    GL.DEPTH_BUFFER_BIT = 0x00000100;

    // Stencil
    GL.STENCIL_TEST = 0x0B90;
    GL.STENCIL_BUFFER_BIT = 0x00000400;
    GL.KEEP = 0x1E00; GL.REPLACE = 0x1E01; GL.INCR = 0x1E02;
    GL.DECR = 0x1E03; GL.INVERT = 0x150A;
    GL.INCR_WRAP = 0x8507; GL.DECR_WRAP = 0x8508;

    // Enable/Disable
    GL.BLEND = 0x0BE2; GL.DITHER = 0x0BD0;
    GL.STENCIL_TEST = 0x0B90; GL.DEPTH_TEST = 0x0B71;
    GL.SCISSOR_TEST = 0x0C11; GL.POLYGON_OFFSET_FILL = 0x8037;
    GL.SAMPLE_ALPHA_TO_COVERAGE = 0x809E; GL.SAMPLE_COVERAGE = 0x80A0;

    // Errors
    GL.NO_ERROR = 0; GL.INVALID_ENUM = 0x0500;
    GL.INVALID_VALUE = 0x0501; GL.INVALID_OPERATION = 0x0502;
    GL.OUT_OF_MEMORY = 0x0505;

    // Framebuffer
    GL.FRAMEBUFFER = 0x8D40; GL.RENDERBUFFER = 0x8D41;
    GL.COLOR_ATTACHMENT0 = 0x8CE0;
    GL.DEPTH_ATTACHMENT = 0x8D00; GL.STENCIL_ATTACHMENT = 0x8D20;
    GL.DEPTH_STENCIL_ATTACHMENT = 0x821A;
    GL.FRAMEBUFFER_COMPLETE = 0x8CD5;
    GL.FRAMEBUFFER_INCOMPLETE_ATTACHMENT = 0x8CD6;
    GL.FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT = 0x8CD7;
    GL.FRAMEBUFFER_INCOMPLETE_DIMENSIONS = 0x8CD9;
    GL.FRAMEBUFFER_UNSUPPORTED = 0x8CDD;
    GL.FRAMEBUFFER_BINDING = 0x8CA6;
    GL.RENDERBUFFER_BINDING = 0x8CA7;
    GL.MAX_RENDERBUFFER_SIZE = 0x84E8;

    // Renderbuffer
    GL.RGBA4 = 0x8056; GL.RGB5_A1 = 0x8057; GL.RGB565 = 0x8D62;
    GL.DEPTH_COMPONENT16 = 0x81A5; GL.STENCIL_INDEX8 = 0x8D48;
    GL.DEPTH_STENCIL = 0x84F9;

    // Textures
    GL.TEXTURE_2D = 0x0DE1; GL.TEXTURE_CUBE_MAP = 0x8513;
    GL.TEXTURE_CUBE_MAP_POSITIVE_X = 0x8515; GL.TEXTURE_CUBE_MAP_NEGATIVE_X = 0x8516;
    GL.TEXTURE_CUBE_MAP_POSITIVE_Y = 0x8517; GL.TEXTURE_CUBE_MAP_NEGATIVE_Y = 0x8518;
    GL.TEXTURE_CUBE_MAP_POSITIVE_Z = 0x8519; GL.TEXTURE_CUBE_MAP_NEGATIVE_Z = 0x851A;
    GL.MAX_CUBE_MAP_TEXTURE_SIZE = 0x851C;
    GL.TEXTURE0 = 0x84C0; GL.TEXTURE1 = 0x84C1; GL.TEXTURE2 = 0x84C2;
    GL.TEXTURE3 = 0x84C3; GL.TEXTURE4 = 0x84C4; GL.TEXTURE5 = 0x84C5;
    GL.TEXTURE6 = 0x84C6; GL.TEXTURE7 = 0x84C7; GL.TEXTURE8 = 0x84C8;
    GL.TEXTURE9 = 0x84C9; GL.TEXTURE10 = 0x84CA; GL.TEXTURE11 = 0x84CB;
    GL.TEXTURE12 = 0x84CC; GL.TEXTURE13 = 0x84CD; GL.TEXTURE14 = 0x84CE;
    GL.TEXTURE15 = 0x84CF;
    GL.ACTIVE_TEXTURE = 0x84E0;
    GL.TEXTURE_MAG_FILTER = 0x2800; GL.TEXTURE_MIN_FILTER = 0x2801;
    GL.TEXTURE_WRAP_S = 0x2802; GL.TEXTURE_WRAP_T = 0x2803;
    GL.NEAREST = 0x2600; GL.LINEAR = 0x2601;
    GL.NEAREST_MIPMAP_NEAREST = 0x2700; GL.LINEAR_MIPMAP_NEAREST = 0x2701;
    GL.NEAREST_MIPMAP_LINEAR = 0x2702; GL.LINEAR_MIPMAP_LINEAR = 0x2703;
    GL.REPEAT = 0x2901; GL.CLAMP_TO_EDGE = 0x812F; GL.MIRRORED_REPEAT = 0x8370;
    GL.MAX_TEXTURE_SIZE = 0x0D33;
    GL.MAX_COMBINED_TEXTURE_IMAGE_UNITS = 0x8B4D;
    GL.MAX_TEXTURE_IMAGE_UNITS = 0x8872;
    GL.MAX_VERTEX_TEXTURE_IMAGE_UNITS = 0x8B4C;

    // Pixel formats
    GL.ALPHA = 0x1906; GL.RGB = 0x1907; GL.RGBA = 0x1908;
    GL.LUMINANCE = 0x1909; GL.LUMINANCE_ALPHA = 0x190A;
    GL.DEPTH_COMPONENT = 0x1902;

    // Pixel storage
    GL.UNPACK_ALIGNMENT = 0x0CF5; GL.PACK_ALIGNMENT = 0x0D05;
    GL.UNPACK_FLIP_Y_WEBGL = 0x9240;
    GL.UNPACK_PREMULTIPLY_ALPHA_WEBGL = 0x9241;
    GL.UNPACK_COLORSPACE_CONVERSION_WEBGL = 0x9243;

    // Shaders
    GL.VERTEX_SHADER = 0x8B31; GL.FRAGMENT_SHADER = 0x8B30;
    GL.COMPILE_STATUS = 0x8B81; GL.LINK_STATUS = 0x8B82;
    GL.VALIDATE_STATUS = 0x8B83; GL.ATTACHED_SHADERS = 0x8B85;
    GL.ACTIVE_UNIFORMS = 0x8B86; GL.ACTIVE_ATTRIBUTES = 0x8B89;
    GL.SHADER_TYPE = 0x8B4F; GL.DELETE_STATUS = 0x8B80;
    GL.MAX_VERTEX_ATTRIBS = 0x8869;
    GL.MAX_VERTEX_UNIFORM_VECTORS = 0x8DFB;
    GL.MAX_VARYING_VECTORS = 0x8DFC;
    GL.MAX_FRAGMENT_UNIFORM_VECTORS = 0x8DFD;
    GL.CURRENT_PROGRAM = 0x8B8D;

    // Uniform types (needed for getActiveUniform)
    GL.FLOAT_VEC2 = 0x8B50; GL.FLOAT_VEC3 = 0x8B51; GL.FLOAT_VEC4 = 0x8B52;
    GL.INT_VEC2 = 0x8B53; GL.INT_VEC3 = 0x8B54; GL.INT_VEC4 = 0x8B55;
    GL.BOOL = 0x8B56; GL.BOOL_VEC2 = 0x8B57; GL.BOOL_VEC3 = 0x8B58;
    GL.BOOL_VEC4 = 0x8B59;
    GL.FLOAT_MAT2 = 0x8B5A; GL.FLOAT_MAT3 = 0x8B5B; GL.FLOAT_MAT4 = 0x8B5C;
    GL.SAMPLER_2D = 0x8B5E; GL.SAMPLER_CUBE = 0x8B60;

    // Clear
    GL.COLOR_BUFFER_BIT = 0x00004000;

    // Misc
    GL.NONE = 0; GL.TRUE = 1; GL.FALSE = 0;
    GL.DONT_CARE = 0x1100; GL.FASTEST = 0x1101; GL.NICEST = 0x1102;
    GL.GENERATE_MIPMAP_HINT = 0x8192;
    GL.VIEWPORT = 0x0BA2;
    GL.SCISSOR_BOX = 0x0C10;
    GL.COLOR_CLEAR_VALUE = 0x0C22;
    GL.COLOR_WRITEMASK = 0x0C23;
    GL.MAX_VIEWPORT_DIMS = 0x0D3A;
    GL.ALIASED_POINT_SIZE_RANGE = 0x846D;
    GL.ALIASED_LINE_WIDTH_RANGE = 0x846E;
    GL.SUBPIXEL_BITS = 0x0D50;
    GL.RED_BITS = 0x0D52; GL.GREEN_BITS = 0x0D53;
    GL.BLUE_BITS = 0x0D54; GL.ALPHA_BITS = 0x0D55;
    GL.DEPTH_BITS = 0x0D56; GL.STENCIL_BITS = 0x0D57;
    GL.RENDERER = 0x1F01; GL.VENDOR = 0x1F00; GL.VERSION = 0x1F02;
    GL.SHADING_LANGUAGE_VERSION = 0x8B8C;
    GL.HIGH_FLOAT = 0x8DF2; GL.MEDIUM_FLOAT = 0x8DF1; GL.LOW_FLOAT = 0x8DF0;
    GL.HIGH_INT = 0x8DF5; GL.MEDIUM_INT = 0x8DF4; GL.LOW_INT = 0x8DF3;

    // Line width
    GL.LINE_WIDTH = 0x0B21;

    // Polygon offset
    GL.POLYGON_OFFSET_FACTOR = 0x8038; GL.POLYGON_OFFSET_UNITS = 0x2A00;

    // Sampling
    GL.SAMPLE_BUFFERS = 0x80A8; GL.SAMPLES = 0x80A9;
    GL.SAMPLE_COVERAGE_VALUE = 0x80AA; GL.SAMPLE_COVERAGE_INVERT = 0x80AB;

    // =========================================================================
    // WebGL2 constants (OpenGL ES 3.0 additions)
    // Defined on window.WebGL2RenderingContext and copied onto webgl2 contexts.
    // =========================================================================
    window.WebGL2RenderingContext = function() {};
    var GL2 = window.WebGL2RenderingContext;

    // Getting GL parameter information
    GL2.READ_BUFFER = 0x0C02;
    GL2.UNPACK_ROW_LENGTH = 0x0CF2; GL2.UNPACK_SKIP_ROWS = 0x0CF3;
    GL2.UNPACK_SKIP_PIXELS = 0x0CF4;
    GL2.PACK_ROW_LENGTH = 0x0D02; GL2.PACK_SKIP_ROWS = 0x0D03;
    GL2.PACK_SKIP_PIXELS = 0x0D04;
    GL2.TEXTURE_BINDING_3D = 0x806A;
    GL2.UNPACK_SKIP_IMAGES = 0x806D; GL2.UNPACK_IMAGE_HEIGHT = 0x806E;
    GL2.MAX_3D_TEXTURE_SIZE = 0x8073;
    GL2.MAX_ELEMENTS_VERTICES = 0x80E8; GL2.MAX_ELEMENTS_INDICES = 0x80E9;
    GL2.MAX_TEXTURE_LOD_BIAS = 0x84FD;
    GL2.MAX_FRAGMENT_UNIFORM_COMPONENTS = 0x8B49;
    GL2.MAX_VERTEX_UNIFORM_COMPONENTS = 0x8B4A;
    GL2.MAX_ARRAY_TEXTURE_LAYERS = 0x88FF;
    GL2.MIN_PROGRAM_TEXEL_OFFSET = 0x8904;
    GL2.MAX_PROGRAM_TEXEL_OFFSET = 0x8905;
    GL2.MAX_VARYING_COMPONENTS = 0x8B4B;
    GL2.FRAGMENT_SHADER_DERIVATIVE_HINT = 0x8B8B;
    GL2.RASTERIZER_DISCARD = 0x8C89;
    GL2.VERTEX_ARRAY_BINDING = 0x85B5;
    GL2.MAX_VERTEX_OUTPUT_COMPONENTS = 0x9122;
    GL2.MAX_FRAGMENT_INPUT_COMPONENTS = 0x9125;
    GL2.MAX_SERVER_WAIT_TIMEOUT = 0x9111;
    GL2.MAX_ELEMENT_INDEX = 0x8D6B;

    // 3D textures
    GL2.TEXTURE_3D = 0x806F; GL2.TEXTURE_WRAP_R = 0x8072;
    GL2.TEXTURE_MIN_LOD = 0x813A; GL2.TEXTURE_MAX_LOD = 0x813B;
    GL2.TEXTURE_BASE_LEVEL = 0x813C; GL2.TEXTURE_MAX_LEVEL = 0x813D;
    GL2.TEXTURE_COMPARE_MODE = 0x884C; GL2.TEXTURE_COMPARE_FUNC = 0x884D;
    GL2.COMPARE_REF_TO_TEXTURE = 0x884E;
    GL2.TEXTURE_2D_ARRAY = 0x8C1A; GL2.TEXTURE_BINDING_2D_ARRAY = 0x8C1D;
    GL2.TEXTURE_IMMUTABLE_FORMAT = 0x912F; GL2.TEXTURE_IMMUTABLE_LEVELS = 0x82DF;

    // sRGB
    GL2.SRGB = 0x8C40; GL2.SRGB8 = 0x8C41; GL2.SRGB8_ALPHA8 = 0x8C43;

    // Sized internal formats
    GL2.RED = 0x1903; GL2.RGB8 = 0x8051; GL2.RGBA8 = 0x8058;
    GL2.RGB10_A2 = 0x8059; GL2.RGB10_A2UI = 0x906F;
    GL2.R8 = 0x8229; GL2.RG8 = 0x822B;
    GL2.R16F = 0x822D; GL2.R32F = 0x822E;
    GL2.RG16F = 0x822F; GL2.RG32F = 0x8230;
    GL2.R8I = 0x8231; GL2.R8UI = 0x8232;
    GL2.R16I = 0x8233; GL2.R16UI = 0x8234;
    GL2.R32I = 0x8235; GL2.R32UI = 0x8236;
    GL2.RG8I = 0x8237; GL2.RG8UI = 0x8238;
    GL2.RG16I = 0x8239; GL2.RG16UI = 0x823A;
    GL2.RG32I = 0x823B; GL2.RG32UI = 0x823C;
    GL2.RGBA32F = 0x8814; GL2.RGB32F = 0x8815;
    GL2.RGBA16F = 0x881A; GL2.RGB16F = 0x881B;
    GL2.R11F_G11F_B10F = 0x8C3A; GL2.RGB9_E5 = 0x8C3D;
    GL2.R8_SNORM = 0x8F94; GL2.RG8_SNORM = 0x8F95;
    GL2.RGB8_SNORM = 0x8F96; GL2.RGBA8_SNORM = 0x8F97;

    // Integer texture formats
    GL2.RGBA32UI = 0x8D70; GL2.RGB32UI = 0x8D71;
    GL2.RGBA16UI = 0x8D76; GL2.RGB16UI = 0x8D77;
    GL2.RGBA8UI = 0x8D7C; GL2.RGB8UI = 0x8D7D;
    GL2.RGBA32I = 0x8D82; GL2.RGB32I = 0x8D83;
    GL2.RGBA16I = 0x8D88; GL2.RGB16I = 0x8D89;
    GL2.RGBA8I = 0x8D8E; GL2.RGB8I = 0x8D8F;
    GL2.RED_INTEGER = 0x8D94; GL2.RGB_INTEGER = 0x8D98;
    GL2.RGBA_INTEGER = 0x8D99;

    // Pixel types
    GL2.UNSIGNED_INT_2_10_10_10_REV = 0x8368;
    GL2.UNSIGNED_INT_10F_11F_11F_REV = 0x8C3B;
    GL2.UNSIGNED_INT_5_9_9_9_REV = 0x8C3E;
    GL2.FLOAT_32_UNSIGNED_INT_24_8_REV = 0x8DAD;
    GL2.UNSIGNED_INT_24_8 = 0x84FA;
    GL2.HALF_FLOAT = 0x140B;
    GL2.RG = 0x8227; GL2.RG_INTEGER = 0x8228;
    GL2.INT_2_10_10_10_REV = 0x8D9F;

    // Uniform Buffer Objects
    GL2.UNIFORM_BUFFER = 0x8A11;
    GL2.UNIFORM_BUFFER_BINDING = 0x8A28;
    GL2.UNIFORM_BUFFER_START = 0x8A29; GL2.UNIFORM_BUFFER_SIZE = 0x8A2A;
    GL2.MAX_VERTEX_UNIFORM_BLOCKS = 0x8A2B;
    GL2.MAX_FRAGMENT_UNIFORM_BLOCKS = 0x8A2D;
    GL2.MAX_COMBINED_UNIFORM_BLOCKS = 0x8A2E;
    GL2.MAX_UNIFORM_BUFFER_BINDINGS = 0x8A2F;
    GL2.MAX_UNIFORM_BLOCK_SIZE = 0x8A30;
    GL2.MAX_COMBINED_VERTEX_UNIFORM_COMPONENTS = 0x8A31;
    GL2.MAX_COMBINED_FRAGMENT_UNIFORM_COMPONENTS = 0x8A33;
    GL2.UNIFORM_BUFFER_OFFSET_ALIGNMENT = 0x8A34;
    GL2.ACTIVE_UNIFORM_BLOCKS = 0x8A36;
    GL2.UNIFORM_TYPE = 0x8A37; GL2.UNIFORM_SIZE = 0x8A38;
    GL2.UNIFORM_BLOCK_INDEX = 0x8A3A;
    GL2.UNIFORM_OFFSET = 0x8A3B;
    GL2.UNIFORM_ARRAY_STRIDE = 0x8A3C; GL2.UNIFORM_MATRIX_STRIDE = 0x8A3D;
    GL2.UNIFORM_IS_ROW_MAJOR = 0x8A3E;
    GL2.UNIFORM_BLOCK_BINDING = 0x8A3F;
    GL2.UNIFORM_BLOCK_DATA_SIZE = 0x8A40;
    GL2.UNIFORM_BLOCK_ACTIVE_UNIFORMS = 0x8A42;
    GL2.UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES = 0x8A43;
    GL2.UNIFORM_BLOCK_REFERENCED_BY_VERTEX_SHADER = 0x8A44;
    GL2.UNIFORM_BLOCK_REFERENCED_BY_FRAGMENT_SHADER = 0x8A46;

    // Transform Feedback
    GL2.TRANSFORM_FEEDBACK_BUFFER_MODE = 0x8C7F;
    GL2.MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS = 0x8C80;
    GL2.TRANSFORM_FEEDBACK_VARYINGS = 0x8C83;
    GL2.TRANSFORM_FEEDBACK_BUFFER_START = 0x8C84;
    GL2.TRANSFORM_FEEDBACK_BUFFER_SIZE = 0x8C85;
    GL2.TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN = 0x8C88;
    GL2.MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS = 0x8C8A;
    GL2.MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS = 0x8C8B;
    GL2.INTERLEAVED_ATTRIBS = 0x8C8C; GL2.SEPARATE_ATTRIBS = 0x8C8D;
    GL2.TRANSFORM_FEEDBACK_BUFFER = 0x8C8E;
    GL2.TRANSFORM_FEEDBACK_BUFFER_BINDING = 0x8C8F;
    GL2.TRANSFORM_FEEDBACK = 0x8E22;
    GL2.TRANSFORM_FEEDBACK_PAUSED = 0x8E23;
    GL2.TRANSFORM_FEEDBACK_ACTIVE = 0x8E24;
    GL2.TRANSFORM_FEEDBACK_BINDING = 0x8E25;

    // Samplers (shader uniform types)
    GL2.SAMPLER_3D = 0x8B5F;
    GL2.SAMPLER_2D_SHADOW = 0x8B62;
    GL2.SAMPLER_2D_ARRAY = 0x8DC1; GL2.SAMPLER_2D_ARRAY_SHADOW = 0x8DC4;
    GL2.SAMPLER_CUBE_SHADOW = 0x8DC5;
    GL2.INT_SAMPLER_2D = 0x8DCA; GL2.INT_SAMPLER_3D = 0x8DCB;
    GL2.INT_SAMPLER_CUBE = 0x8DCC; GL2.INT_SAMPLER_2D_ARRAY = 0x8DCF;
    GL2.UNSIGNED_INT_SAMPLER_2D = 0x8DD2; GL2.UNSIGNED_INT_SAMPLER_3D = 0x8DD3;
    GL2.UNSIGNED_INT_SAMPLER_CUBE = 0x8DD4; GL2.UNSIGNED_INT_SAMPLER_2D_ARRAY = 0x8DD7;
    GL2.MAX_SAMPLES = 0x8D57; GL2.SAMPLER_BINDING = 0x8919;

    // Queries
    GL2.CURRENT_QUERY = 0x8865;
    GL2.QUERY_RESULT = 0x8866; GL2.QUERY_RESULT_AVAILABLE = 0x8867;
    GL2.ANY_SAMPLES_PASSED = 0x8C2F;
    GL2.ANY_SAMPLES_PASSED_CONSERVATIVE = 0x8D6A;

    // Draw buffers / MRT
    GL2.MAX_DRAW_BUFFERS = 0x8824; GL2.MAX_COLOR_ATTACHMENTS = 0x8CDF;
    GL2.DRAW_BUFFER0 = 0x8825; GL2.DRAW_BUFFER1 = 0x8826;
    GL2.DRAW_BUFFER2 = 0x8827; GL2.DRAW_BUFFER3 = 0x8828;
    GL2.DRAW_BUFFER4 = 0x8829; GL2.DRAW_BUFFER5 = 0x882A;
    GL2.DRAW_BUFFER6 = 0x882B; GL2.DRAW_BUFFER7 = 0x882C;
    GL2.DRAW_BUFFER8 = 0x882D; GL2.DRAW_BUFFER9 = 0x882E;
    GL2.DRAW_BUFFER10 = 0x882F; GL2.DRAW_BUFFER11 = 0x8830;
    GL2.DRAW_BUFFER12 = 0x8831; GL2.DRAW_BUFFER13 = 0x8832;
    GL2.DRAW_BUFFER14 = 0x8833; GL2.DRAW_BUFFER15 = 0x8834;
    GL2.COLOR_ATTACHMENT1 = 0x8CE1; GL2.COLOR_ATTACHMENT2 = 0x8CE2;
    GL2.COLOR_ATTACHMENT3 = 0x8CE3; GL2.COLOR_ATTACHMENT4 = 0x8CE4;
    GL2.COLOR_ATTACHMENT5 = 0x8CE5; GL2.COLOR_ATTACHMENT6 = 0x8CE6;
    GL2.COLOR_ATTACHMENT7 = 0x8CE7; GL2.COLOR_ATTACHMENT8 = 0x8CE8;
    GL2.COLOR_ATTACHMENT9 = 0x8CE9; GL2.COLOR_ATTACHMENT10 = 0x8CEA;
    GL2.COLOR_ATTACHMENT11 = 0x8CEB; GL2.COLOR_ATTACHMENT12 = 0x8CEC;
    GL2.COLOR_ATTACHMENT13 = 0x8CED; GL2.COLOR_ATTACHMENT14 = 0x8CEE;
    GL2.COLOR_ATTACHMENT15 = 0x8CEF;

    // Sync objects
    GL2.OBJECT_TYPE = 0x9112; GL2.SYNC_CONDITION = 0x9113;
    GL2.SYNC_STATUS = 0x9114; GL2.SYNC_FLAGS = 0x9115;
    GL2.SYNC_FENCE = 0x9116; GL2.SYNC_GPU_COMMANDS_COMPLETE = 0x9117;
    GL2.UNSIGNALED = 0x9118; GL2.SIGNALED = 0x9119;
    GL2.ALREADY_SIGNALED = 0x911A; GL2.TIMEOUT_EXPIRED = 0x911B;
    GL2.CONDITION_SATISFIED = 0x911C; GL2.WAIT_FAILED = 0x911D;
    GL2.SYNC_FLUSH_COMMANDS_BIT = 0x00000001;

    // Framebuffer / Renderbuffer (new in WebGL2)
    GL2.FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING = 0x8210;
    GL2.FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE = 0x8211;
    GL2.FRAMEBUFFER_ATTACHMENT_RED_SIZE = 0x8212;
    GL2.FRAMEBUFFER_ATTACHMENT_GREEN_SIZE = 0x8213;
    GL2.FRAMEBUFFER_ATTACHMENT_BLUE_SIZE = 0x8214;
    GL2.FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE = 0x8215;
    GL2.FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE = 0x8216;
    GL2.FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE = 0x8217;
    GL2.FRAMEBUFFER_DEFAULT = 0x8218;
    GL2.DEPTH24_STENCIL8 = 0x88F0;
    GL2.DRAW_FRAMEBUFFER_BINDING = 0x8CA6;
    GL2.READ_FRAMEBUFFER = 0x8CA8; GL2.DRAW_FRAMEBUFFER = 0x8CA9;
    GL2.READ_FRAMEBUFFER_BINDING = 0x8CAA;
    GL2.RENDERBUFFER_SAMPLES = 0x8CAB;
    GL2.FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER = 0x8CD4;
    GL2.FRAMEBUFFER_INCOMPLETE_MULTISAMPLE = 0x8D56;

    // Buffer targets (new in WebGL2)
    GL2.PIXEL_PACK_BUFFER = 0x88EB; GL2.PIXEL_UNPACK_BUFFER = 0x88EC;
    GL2.PIXEL_PACK_BUFFER_BINDING = 0x88ED;
    GL2.PIXEL_UNPACK_BUFFER_BINDING = 0x88EF;
    GL2.COPY_READ_BUFFER = 0x8F36; GL2.COPY_WRITE_BUFFER = 0x8F37;
    GL2.COPY_READ_BUFFER_BINDING = 0x8F36; GL2.COPY_WRITE_BUFFER_BINDING = 0x8F37;

    // Data types (new in WebGL2)
    GL2.FLOAT_MAT2x3 = 0x8B65; GL2.FLOAT_MAT2x4 = 0x8B66;
    GL2.FLOAT_MAT3x2 = 0x8B67; GL2.FLOAT_MAT3x4 = 0x8B68;
    GL2.FLOAT_MAT4x2 = 0x8B69; GL2.FLOAT_MAT4x3 = 0x8B6A;
    GL2.UNSIGNED_INT_VEC2 = 0x8DC6; GL2.UNSIGNED_INT_VEC3 = 0x8DC7;
    GL2.UNSIGNED_INT_VEC4 = 0x8DC8;
    GL2.UNSIGNED_NORMALIZED = 0x8C17; GL2.SIGNED_NORMALIZED = 0x8F9C;

    // Vertex attribute constants (new in WebGL2)
    GL2.VERTEX_ATTRIB_ARRAY_INTEGER = 0x88FD;
    GL2.VERTEX_ATTRIB_ARRAY_DIVISOR = 0x88FE;

    // Miscellaneous (new in WebGL2)
    GL2.COLOR = 0x1800; GL2.DEPTH = 0x1801; GL2.STENCIL = 0x1802;
    GL2.MIN = 0x8007; GL2.MAX = 0x8008;
    GL2.DEPTH_COMPONENT24 = 0x81A6;
    GL2.DEPTH_COMPONENT32F = 0x8CAC; GL2.DEPTH32F_STENCIL8 = 0x8CAD;
    GL2.STREAM_READ = 0x88E1; GL2.STREAM_COPY = 0x88E2;
    GL2.STATIC_READ = 0x88E5; GL2.STATIC_COPY = 0x88E6;
    GL2.DYNAMIC_READ = 0x88E9; GL2.DYNAMIC_COPY = 0x88EA;
    GL2.INVALID_INDEX = 0xFFFFFFFF;
    GL2.TIMEOUT_IGNORED = -1;
    GL2.MAX_CLIENT_WAIT_TIMEOUT_WEBGL = 0x9247;

    // =========================================================================
    // Keyboard event shim
    // Ultralight's FireKeyEvent does not populate DOM KeyboardEvent properties
    // (key, code, keyCode) in this version. We work around this by having C++
    // set window.__prismaKeyInfo before each FireKeyEvent call, then a
    // capture-phase listener enriches the empty native event using
    // Object.defineProperty before any application handler sees it.
    // =========================================================================

    var __prismaVKMap = {
        8: {key: 'Backspace', code: 'Backspace'},
        9: {key: 'Tab', code: 'Tab'},
        13: {key: 'Enter', code: 'Enter'},
        16: {key: 'Shift', code: 'ShiftLeft'},
        17: {key: 'Control', code: 'ControlLeft'},
        18: {key: 'Alt', code: 'AltLeft'},
        19: {key: 'Pause', code: 'Pause'},
        20: {key: 'CapsLock', code: 'CapsLock'},
        27: {key: 'Escape', code: 'Escape'},
        32: {key: ' ', code: 'Space'},
        33: {key: 'PageUp', code: 'PageUp'},
        34: {key: 'PageDown', code: 'PageDown'},
        35: {key: 'End', code: 'End'},
        36: {key: 'Home', code: 'Home'},
        37: {key: 'ArrowLeft', code: 'ArrowLeft'},
        38: {key: 'ArrowUp', code: 'ArrowUp'},
        39: {key: 'ArrowRight', code: 'ArrowRight'},
        40: {key: 'ArrowDown', code: 'ArrowDown'},
        45: {key: 'Insert', code: 'Insert'},
        46: {key: 'Delete', code: 'Delete'},
        91: {key: 'Meta', code: 'MetaLeft'},
        92: {key: 'Meta', code: 'MetaRight'},
        93: {key: 'ContextMenu', code: 'ContextMenu'},
        112: {key: 'F1', code: 'F1'}, 113: {key: 'F2', code: 'F2'},
        114: {key: 'F3', code: 'F3'}, 115: {key: 'F4', code: 'F4'},
        116: {key: 'F5', code: 'F5'}, 117: {key: 'F6', code: 'F6'},
        118: {key: 'F7', code: 'F7'}, 119: {key: 'F8', code: 'F8'},
        120: {key: 'F9', code: 'F9'}, 121: {key: 'F10', code: 'F10'},
        122: {key: 'F11', code: 'F11'}, 123: {key: 'F12', code: 'F12'},
        144: {key: 'NumLock', code: 'NumLock'},
        145: {key: 'ScrollLock', code: 'ScrollLock'},
        186: {key: ';', code: 'Semicolon'},
        187: {key: '=', code: 'Equal'},
        188: {key: ',', code: 'Comma'},
        189: {key: '-', code: 'Minus'},
        190: {key: '.', code: 'Period'},
        191: {key: '/', code: 'Slash'},
        192: {key: '`', code: 'Backquote'},
        219: {key: '[', code: 'BracketLeft'},
        220: {key: '\\', code: 'Backslash'},
        221: {key: ']', code: 'BracketRight'},
        222: {key: "'", code: 'Quote'}
    };

    // A-Z (65-90)
    for (var i = 65; i <= 90; i++) {
        __prismaVKMap[i] = {key: String.fromCharCode(i + 32), code: 'Key' + String.fromCharCode(i)};
    }
    // 0-9 (48-57)
    for (var i = 48; i <= 57; i++) {
        __prismaVKMap[i] = {key: String(i - 48), code: 'Digit' + String(i - 48)};
    }
    // Numpad 0-9 (96-105)
    for (var i = 96; i <= 105; i++) {
        __prismaVKMap[i] = {key: String(i - 96), code: 'Numpad' + String(i - 96)};
    }
    // Numpad operators
    __prismaVKMap[106] = {key: '*', code: 'NumpadMultiply'};
    __prismaVKMap[107] = {key: '+', code: 'NumpadAdd'};
    __prismaVKMap[109] = {key: '-', code: 'NumpadSubtract'};
    __prismaVKMap[110] = {key: '.', code: 'NumpadDecimal'};
    __prismaVKMap[111] = {key: '/', code: 'NumpadDivide'};

    // EvaluateScript runs in the main frame context, so __prismaKeyInfo is
    // set on the top window.  Subframes must read from window.top to find it.
    var __prismaKeyInfoOwner = (function() {
        try { return window.top || window; } catch(e) { return window; }
    })();

    function __prismaEnrichKeyEvent(e) {
        var info = __prismaKeyInfoOwner.__prismaKeyInfo;
        if (!info) return;

        var mapped = __prismaVKMap[info.vk];
        if (!mapped) return;

        var shiftKey = !!(info.mods & 8);
        var ctrlKey = !!(info.mods & 2);
        var altKey = !!(info.mods & 1);
        var metaKey = !!(info.mods & 4);

        var key = mapped.key;
        // Shift transforms single-char letter keys to uppercase
        if (key.length === 1 && shiftKey && key >= 'a' && key <= 'z') {
            key = key.toUpperCase();
        }

        Object.defineProperty(e, 'key', {value: key, configurable: true});
        Object.defineProperty(e, 'code', {value: mapped.code, configurable: true});
        Object.defineProperty(e, 'keyCode', {value: info.vk, configurable: true});
        Object.defineProperty(e, 'which', {value: info.vk, configurable: true});
        Object.defineProperty(e, 'shiftKey', {value: shiftKey, configurable: true});
        Object.defineProperty(e, 'ctrlKey', {value: ctrlKey, configurable: true});
        Object.defineProperty(e, 'altKey', {value: altKey, configurable: true});
        Object.defineProperty(e, 'metaKey', {value: metaKey, configurable: true});

        __prismaKeyInfoOwner.__prismaKeyInfo = null;
    }

    document.addEventListener('keydown', __prismaEnrichKeyEvent, true);
    document.addEventListener('keyup', __prismaEnrichKeyEvent, true);

    console.log('[PrismaUI] WebGL shim loaded');
})();
