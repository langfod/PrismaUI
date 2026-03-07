(function() {
    'use strict';

    // Re-injection guard
    if (window.__prismaWebAudioShimLoaded) return;
    window.__prismaWebAudioShimLoaded = true;

    // ---- AudioParam wrapper ----
    // Wraps a native AudioParam JSC object (with value get/set and scheduling methods)
    function wrapAudioParam(nativeParam) {
        if (!nativeParam) {
            // Return a stub param if native is missing
            return {
                value: 0, defaultValue: 0,
                minValue: -3.4028235e+38, maxValue: 3.4028235e+38,
                setValueAtTime: function() { return this; },
                linearRampToValueAtTime: function() { return this; },
                exponentialRampToValueAtTime: function() { return this; },
                setTargetAtTime: function() { return this; },
                setValueCurveAtTime: function() { return this; },
                cancelScheduledValues: function() { return this; },
                cancelAndHoldAtTime: function() { return this; }
            };
        }
        // The native object already has value, defaultValue, minValue, maxValue as staticValues
        // and scheduling methods as staticFunctions. We can return it directly,
        // but wrap in a thin object to add cancelAndHoldAtTime (not in native).
        var wrapper = Object.create(nativeParam);
        wrapper.cancelAndHoldAtTime = function() { return wrapper; };
        return wrapper;
    }

    // ---- Stub audio listener (Tier 2) ----
    function StubAudioListener() {
        this.positionX = wrapAudioParam(null);
        this.positionY = wrapAudioParam(null);
        this.positionZ = wrapAudioParam(null);
        this.forwardX = wrapAudioParam(null);
        this.forwardY = wrapAudioParam(null);
        this.forwardZ = wrapAudioParam(null);
        this.upX = wrapAudioParam(null);
        this.upY = wrapAudioParam(null);
        this.upZ = wrapAudioParam(null);
    }
    StubAudioListener.prototype.setPosition = function() {};
    StubAudioListener.prototype.setOrientation = function() {};

    // ---- Stub node for unimplemented types (Tier 2) ----
    function stubNode(name, ctx) {
        return {
            connect: function(d) { return d; },
            disconnect: function() {},
            context: ctx,
            numberOfInputs: 1,
            numberOfOutputs: 1,
            channelCount: 2,
            channelCountMode: 'max',
            channelInterpretation: 'speakers',
            addEventListener: function() {},
            removeEventListener: function() {}
        };
    }

    // ---- PrismaAudioContext ----
    function PrismaAudioContext(options) {
        var sr = (options && options.sampleRate) || 0;

        if (typeof __prismaCreateAudioContext !== 'function') {
            throw new Error('PrismaUI audio bridge not available');
        }

        this._native = __prismaCreateAudioContext(sr);
        if (!this._native) {
            throw new Error('Failed to create AudioContext');
        }

        this._listener = new StubAudioListener();
        this._endedCheckInterval = null;
        this._activeSources = [];

        // Start polling for ended events
        var self = this;
        this._endedCheckInterval = setInterval(function() {
            for (var i = self._activeSources.length - 1; i >= 0; i--) {
                var entry = self._activeSources[i];
                if (entry.nativeNode && entry.nativeNode.ended) {
                    self._activeSources.splice(i, 1);
                    if (typeof entry.onended === 'function') {
                        try { entry.onended(); } catch(e) {}
                    }
                }
            }
        }, 50);
    }

    Object.defineProperties(PrismaAudioContext.prototype, {
        sampleRate:  { get: function() { return this._native ? this._native.sampleRate : 0; } },
        currentTime: { get: function() { return this._native ? this._native.currentTime : 0; } },
        state:       { get: function() { return this._native ? this._native.state : 'closed'; } },
        baseLatency: { get: function() { return this._native ? this._native.baseLatency : 0.01; } },
        destination: { get: function() { return this._native ? this._native.destination : null; } },
        listener:    { get: function() { return this._listener; } }
    });

    PrismaAudioContext.prototype.resume = function() {
        if (this._native) this._native.resume();
        return Promise.resolve();
    };

    PrismaAudioContext.prototype.suspend = function() {
        if (this._native) this._native.suspend();
        return Promise.resolve();
    };

    PrismaAudioContext.prototype.close = function() {
        if (this._endedCheckInterval) {
            clearInterval(this._endedCheckInterval);
            this._endedCheckInterval = null;
        }
        if (this._native) this._native.close();
        return Promise.resolve();
    };

    PrismaAudioContext.prototype.createGain = function() {
        if (!this._native) return stubNode('GainNode', this);
        return this._native.createGain();
    };

    PrismaAudioContext.prototype.createBufferSource = function() {
        if (!this._native) return stubNode('AudioBufferSourceNode', this);
        var nativeNode = this._native.createBufferSource();
        var self = this;

        // Wrap start() to track active sources for onended polling
        var origStart = nativeNode.start.bind(nativeNode);
        var entry = { nativeNode: nativeNode, onended: null };

        nativeNode.start = function(when, offset, duration) {
            origStart(when || 0, offset || 0, duration || -1);
            self._activeSources.push(entry);
        };

        // Allow setting onended
        Object.defineProperty(nativeNode, 'onended', {
            get: function() { return entry.onended; },
            set: function(fn) { entry.onended = fn; },
            configurable: true
        });

        return nativeNode;
    };

    PrismaAudioContext.prototype.createBuffer = function(numChannels, length, sampleRate) {
        if (!this._native) return null;
        return this._native.createBuffer(numChannels, length, sampleRate);
    };

    PrismaAudioContext.prototype.decodeAudioData = function(arrayBuffer, successCb, errorCb) {
        if (!this._native) {
            var err = new Error('AudioContext not available');
            if (errorCb) try { errorCb(err); } catch(e) {}
            return Promise.reject(err);
        }
        try {
            var nativeBuf = this._native.decodeAudioData(arrayBuffer);
            if (successCb) try { successCb(nativeBuf); } catch(e) {}
            return Promise.resolve(nativeBuf);
        } catch(e) {
            if (errorCb) try { errorCb(e); } catch(e2) {}
            return Promise.reject(e);
        }
    };

    // Tier 2 stubs — log once but don't crash
    PrismaAudioContext.prototype.createOscillator = function() {
        console.warn('[WebAudio] createOscillator not yet implemented');
        return stubNode('OscillatorNode', this);
    };
    PrismaAudioContext.prototype.createBiquadFilter = function() {
        console.warn('[WebAudio] createBiquadFilter not yet implemented');
        return stubNode('BiquadFilterNode', this);
    };
    PrismaAudioContext.prototype.createDynamicsCompressor = function() {
        console.warn('[WebAudio] createDynamicsCompressor not yet implemented');
        return stubNode('DynamicsCompressorNode', this);
    };
    PrismaAudioContext.prototype.createAnalyser = function() {
        console.warn('[WebAudio] createAnalyser not yet implemented');
        var node = stubNode('AnalyserNode', this);
        node.fftSize = 2048;
        node.frequencyBinCount = 1024;
        node.minDecibels = -100;
        node.maxDecibels = -30;
        node.smoothingTimeConstant = 0.8;
        node.getFloatFrequencyData = function(arr) { if (arr) arr.fill(0); };
        node.getByteFrequencyData = function(arr) { if (arr) arr.fill(0); };
        node.getFloatTimeDomainData = function(arr) { if (arr) arr.fill(0); };
        node.getByteTimeDomainData = function(arr) { if (arr) arr.fill(128); };
        return node;
    };
    PrismaAudioContext.prototype.createChannelMerger = function() {
        return stubNode('ChannelMergerNode', this);
    };
    PrismaAudioContext.prototype.createChannelSplitter = function() {
        return stubNode('ChannelSplitterNode', this);
    };
    PrismaAudioContext.prototype.createScriptProcessor = function(bufSize) {
        console.warn('[WebAudio] createScriptProcessor not yet implemented');
        var node = stubNode('ScriptProcessorNode', this);
        node.bufferSize = bufSize || 4096;
        node.onaudioprocess = null;
        return node;
    };
    PrismaAudioContext.prototype.createPanner = function() {
        console.warn('[WebAudio] createPanner not yet implemented');
        return stubNode('PannerNode', this);
    };
    PrismaAudioContext.prototype.createStereoPanner = function() {
        console.warn('[WebAudio] createStereoPanner not yet implemented');
        var node = stubNode('StereoPannerNode', this);
        node.pan = wrapAudioParam(null);
        return node;
    };
    PrismaAudioContext.prototype.createConvolver = function() {
        console.warn('[WebAudio] createConvolver not yet implemented');
        return stubNode('ConvolverNode', this);
    };
    PrismaAudioContext.prototype.createDelay = function() {
        console.warn('[WebAudio] createDelay not yet implemented');
        var node = stubNode('DelayNode', this);
        node.delayTime = wrapAudioParam(null);
        return node;
    };
    PrismaAudioContext.prototype.createWaveShaper = function() {
        console.warn('[WebAudio] createWaveShaper not yet implemented');
        return stubNode('WaveShaperNode', this);
    };
    PrismaAudioContext.prototype.createPeriodicWave = function() {
        return {};
    };
    PrismaAudioContext.prototype.createMediaElementSource = function(elem) {
        console.warn('[WebAudio] createMediaElementSource not yet implemented');
        return stubNode('MediaElementAudioSourceNode', this);
    };

    // Expose on window
    window.AudioContext = PrismaAudioContext;
    window.webkitAudioContext = PrismaAudioContext;

    // ---- HTMLAudioElement (new Audio()) ----
    var _defaultAudioCtx = null;
    function getDefaultAudioContext() {
        if (!_defaultAudioCtx || _defaultAudioCtx.state === 'closed') {
            try {
                _defaultAudioCtx = new PrismaAudioContext();
                _defaultAudioCtx.resume();
            } catch(e) {
                return null;
            }
        }
        return _defaultAudioCtx;
    }

    function PrismaAudio(src) {
        this.src = src || '';
        this._volume = 1.0;
        this.currentTime = 0;
        this.duration = 0;
        this.paused = true;
        this.ended = false;
        this.muted = false;
        this.loop = false;
        this.autoplay = false;
        this.preload = 'auto';
        this.readyState = 0;
        this._listeners = {};
        this._buffer = null;
        this._source = null;
        this._gainNode = null;
        this._loaded = false;
        this._loading = false;
        this._ctx = null;
        this._playbackOffset = 0;
        this._playbackStartedAt = 0;
    }

    Object.defineProperty(PrismaAudio.prototype, 'volume', {
        get: function() { return this._volume; },
        set: function(v) {
            this._volume = Math.max(0, Math.min(1, v));
            if (this._gainNode) {
                this._gainNode.gain.value = this.muted ? 0 : this._volume;
            }
        }
    });

    PrismaAudio.prototype._emit = function(evt) {
        if (!this._listeners[evt]) return;
        var fns = this._listeners[evt].slice();
        for (var i = 0; i < fns.length; i++) {
            try { fns[i].call(this); } catch(e) {}
        }
    };

    PrismaAudio.prototype.addEventListener = function(evt, fn) {
        if (!this._listeners[evt]) this._listeners[evt] = [];
        this._listeners[evt].push(fn);
    };

    PrismaAudio.prototype.removeEventListener = function(evt, fn) {
        if (!this._listeners[evt]) return;
        this._listeners[evt] = this._listeners[evt].filter(function(f) { return f !== fn; });
    };

    PrismaAudio.prototype.load = function() {
        if (this._loading || this._loaded || !this.src) return;
        this._loading = true;
        var self = this;
        var ctx = getDefaultAudioContext();
        if (!ctx) { this._loading = false; return; }

        fetch(this.src)
            .then(function(resp) {
                if (!resp.ok) throw new Error('HTTP ' + resp.status);
                return resp.arrayBuffer();
            })
            .then(function(ab) {
                return ctx.decodeAudioData(ab);
            })
            .then(function(buffer) {
                self._buffer = buffer;
                self.duration = buffer.duration;
                self._loaded = true;
                self._loading = false;
                self.readyState = 4;
                self._emit('canplaythrough');
                self._emit('loadeddata');
                if (self.autoplay) self.play();
            })
            .catch(function(e) {
                self._loading = false;
                console.error('[Audio] Failed to load: ' + self.src, e);
                self._emit('error');
            });
    };

    PrismaAudio.prototype._startPlayback = function(ctx) {
        if (this._source) {
            try { this._source.stop(); } catch(e) {}
        }

        this._source = ctx.createBufferSource();
        this._source.buffer = this._buffer;
        this._source.loop = this.loop;

        this._gainNode = ctx.createGain();
        this._gainNode.gain.value = this.muted ? 0 : this._volume;

        this._source.connect(this._gainNode);
        this._gainNode.connect(ctx.destination);

        this._ctx = ctx;
        this._playbackOffset = this.currentTime;

        var self = this;
        this._source.onended = function() {
            self.ended = true;
            self.paused = true;
            self._playbackStartedAt = 0;
            self.currentTime = 0;
            self._emit('ended');
        };

        this._source.start(0, this.currentTime);
        this._playbackStartedAt = ctx.currentTime;
        this.paused = false;
        this.ended = false;
    };

    PrismaAudio.prototype.play = function() {
        var self = this;
        var ctx = getDefaultAudioContext();
        if (!ctx) return Promise.reject(new Error('No audio context'));

        if (!this._loaded) {
            return new Promise(function(resolve, reject) {
                self.load();
                var onReady = function() {
                    self.removeEventListener('canplaythrough', onReady);
                    self.removeEventListener('error', onError);
                    self._startPlayback(ctx);
                    resolve();
                };
                var onError = function() {
                    self.removeEventListener('canplaythrough', onReady);
                    self.removeEventListener('error', onError);
                    reject(new Error('Failed to load audio'));
                };
                self.addEventListener('canplaythrough', onReady);
                self.addEventListener('error', onError);
            });
        }

        this._startPlayback(ctx);
        return Promise.resolve();
    };

    PrismaAudio.prototype.pause = function() {
        if (this._source) {
            if (this._playbackStartedAt > 0 && this._ctx) {
                this.currentTime = this._playbackOffset + (this._ctx.currentTime - this._playbackStartedAt);
            }
            this._playbackStartedAt = 0;
            try { this._source.stop(); } catch(e) {}
            this._source = null;
        }
        this.paused = true;
    };

    PrismaAudio.prototype.canPlayType = function(type) {
        if (!type) return '';
        type = type.toLowerCase();
        if (type.indexOf('audio/mpeg') >= 0 || type.indexOf('audio/mp3') >= 0) return 'probably';
        if (type.indexOf('audio/ogg') >= 0 || type.indexOf('audio/vorbis') >= 0) return 'probably';
        if (type.indexOf('audio/wav') >= 0 || type.indexOf('audio/wave') >= 0) return 'probably';
        if (type.indexOf('audio/flac') >= 0) return 'probably';
        return '';
    };

    PrismaAudio.prototype.cloneNode = function() { return new PrismaAudio(this.src); };
    PrismaAudio.prototype.dispatchEvent = function() { return true; };

    window.Audio = PrismaAudio;

    console.log('[PrismaUI] Web Audio shim loaded');
})();
