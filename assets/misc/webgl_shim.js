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

        var x = rect.left || 0;
        var y = rect.top || 0;
        var w = canvas.width || rect.width || 0;
        var h = canvas.height || rect.height || 0;
        var visible = true;

        if (__prismaIsHidden(canvas)) {
            visible = false;
        }

        var win = canvas.ownerDocument && canvas.ownerDocument.defaultView;
        while (win && win.frameElement) {
            var fe = win.frameElement;
            if (__prismaIsHidden(fe)) {
                visible = false;
            }
            if (typeof fe.getBoundingClientRect === 'function') {
                var fr = fe.getBoundingClientRect();
                x += fr.left || 0;
                y += fr.top || 0;
            }
            win = win.parent;
        }

        if (w <= 0 || h <= 0 || rect.width === 0 || rect.height === 0) {
            visible = false;
        }

        return { x: x, y: y, w: w, h: h, visible: visible };
    }

    function __prismaUpdateAllWebGL() {
        if (typeof __prismaUpdateWebGLContext !== 'function') return;
        if (!window.__prismaWebGLContexts) return;
        for (var i = 0; i < window.__prismaWebGLContexts.length; i++) {
            var ctx = window.__prismaWebGLContexts[i];
            if (!ctx || !ctx.canvas) continue;
            var info = __prismaComputeCanvasInfo(ctx.canvas);
            __prismaUpdateWebGLContext(ctx, info.x, info.y, info.w, info.h, info.visible);
        }
    }

    HTMLCanvasElement.prototype.getContext = function(type, attrs) {
        if (type === 'webgl' || type === 'experimental-webgl' || type === 'webgl2') {
            if (type === 'webgl2') {
                console.warn('[WebGL] webgl2 requested but only webgl1 is available — returning webgl1 context');
            }

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

                    // Wrap getExtension to log unsupported extension requests
                    var _origGetExtension = ctx.getExtension.bind(ctx);
                    ctx.getExtension = function(name) {
                        var ext = _origGetExtension(name);
                        if (!ext) {
                            console.info('[WebGL] Extension not available: ' + name);
                        }
                        return ext;
                    };

                    // Wrap context in Proxy to log calls to unimplemented methods
                    const proxied = new Proxy(ctx, {
                        get(target, prop, receiver) {
                            if (prop in target) {
                                return Reflect.get(target, prop, receiver);
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
                    window.__prismaWebGLContexts[window.__prismaWebGLContexts.length - 1] = proxied;

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
    // Iframe support: auto-assign name attributes so C++ can target subframes.
    // Ultralight's LockJSContext/EvaluateScript require the iframe 'name' attr.
    // =========================================================================
    function nameIframes() {
        var iframes = document.querySelectorAll('iframe');
        for (var i = 0; i < iframes.length; i++) {
            if (!iframes[i].name) {
                iframes[i].name = '__prisma_frame_' + i;
            }
        }
    }
    nameIframes();

    // Watch for dynamically added iframes and name them immediately
    if (typeof MutationObserver !== 'undefined' && document.documentElement) {
        new MutationObserver(function(mutations) {
            var needsNaming = false;
            mutations.forEach(function(m) {
                m.addedNodes.forEach(function(n) {
                    if (n.tagName === 'IFRAME' || (n.querySelectorAll && n.querySelectorAll('iframe').length)) {
                        needsNaming = true;
                    }
                });
            });
            if (needsNaming) nameIframes();
        }).observe(document.documentElement, { childList: true, subtree: true });
    }

    console.log('[PrismaUI] WebGL shim loaded');
})();
