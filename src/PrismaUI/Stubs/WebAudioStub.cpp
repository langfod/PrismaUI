#include "WebAudioStub.h"

namespace PrismaUI::Stubs {

    const char* GetWebAudioStubJS() {
        // ---------------------------------------------------------------
        // Minimal no-op Web Audio API + HTMLAudioElement stubs.
        // Everything is wrapped in an IIFE; bails out immediately when
        // the real AudioContext already exists.
        // ---------------------------------------------------------------
        return R"JS(
(function() {
    "use strict";
    if (typeof AudioContext !== 'undefined') return;

    // ---- AudioParam stub ----
    function StubAudioParam(v) {
        this.value = v || 0;
        this.defaultValue = this.value;
        this.minValue = -3.4028235e+38;
        this.maxValue = 3.4028235e+38;
    }
    StubAudioParam.prototype.setValueAtTime             = function(v) { this.value = v; return this; };
    StubAudioParam.prototype.linearRampToValueAtTime     = function()  { return this; };
    StubAudioParam.prototype.exponentialRampToValueAtTime = function() { return this; };
    StubAudioParam.prototype.setTargetAtTime             = function()  { return this; };
    StubAudioParam.prototype.setValueCurveAtTime         = function()  { return this; };
    StubAudioParam.prototype.cancelScheduledValues       = function()  { return this; };

    // ---- AudioNode base ----
    function StubAudioNode() {
        this.numberOfInputs  = 0;
        this.numberOfOutputs = 1;
        this.channelCount = 2;
        this.channelCountMode = 'max';
        this.channelInterpretation = 'speakers';
        this.context = null;
    }
    StubAudioNode.prototype.connect    = function(dest) { return dest; };
    StubAudioNode.prototype.disconnect = function() {};

    // ---- AudioDestinationNode ----
    function StubDestination(ctx) {
        StubAudioNode.call(this);
        this.context = ctx;
        this.maxChannelCount = 2;
        this.numberOfInputs  = 1;
        this.numberOfOutputs = 0;
    }
    StubDestination.prototype = Object.create(StubAudioNode.prototype);

    // ---- GainNode ----
    function StubGainNode(ctx) {
        StubAudioNode.call(this);
        this.context = ctx;
        this.gain = new StubAudioParam(1.0);
    }
    StubGainNode.prototype = Object.create(StubAudioNode.prototype);

    // ---- BiquadFilterNode ----
    function StubBiquadFilterNode(ctx) {
        StubAudioNode.call(this);
        this.context = ctx;
        this.type = 'lowpass';
        this.frequency = new StubAudioParam(350);
        this.detune    = new StubAudioParam(0);
        this.Q         = new StubAudioParam(1);
        this.gain      = new StubAudioParam(0);
    }
    StubBiquadFilterNode.prototype = Object.create(StubAudioNode.prototype);
    StubBiquadFilterNode.prototype.getFrequencyResponse = function() {};

    // ---- DynamicsCompressorNode ----
    function StubDynamicsCompressorNode(ctx) {
        StubAudioNode.call(this);
        this.context   = ctx;
        this.threshold = new StubAudioParam(-24);
        this.knee      = new StubAudioParam(30);
        this.ratio     = new StubAudioParam(12);
        this.reduction = 0;
        this.attack    = new StubAudioParam(0.003);
        this.release   = new StubAudioParam(0.25);
    }
    StubDynamicsCompressorNode.prototype = Object.create(StubAudioNode.prototype);

    // ---- OscillatorNode ----
    function StubOscillatorNode(ctx) {
        StubAudioNode.call(this);
        this.context   = ctx;
        this.type      = 'sine';
        this.frequency = new StubAudioParam(440);
        this.detune    = new StubAudioParam(0);
        this.onended   = null;
    }
    StubOscillatorNode.prototype = Object.create(StubAudioNode.prototype);
    StubOscillatorNode.prototype.start       = function() {};
    StubOscillatorNode.prototype.stop        = function() {};
    StubOscillatorNode.prototype.setPeriodicWave = function() {};

    // ---- AudioBufferSourceNode ----
    function StubBufferSource(ctx) {
        StubAudioNode.call(this);
        this.context      = ctx;
        this.buffer       = null;
        this.loop         = false;
        this.loopStart    = 0;
        this.loopEnd      = 0;
        this.playbackRate = new StubAudioParam(1.0);
        this.detune       = new StubAudioParam(0);
        this.onended      = null;
    }
    StubBufferSource.prototype = Object.create(StubAudioNode.prototype);
    StubBufferSource.prototype.start = function() {};
    StubBufferSource.prototype.stop  = function() {};

    // ---- ScriptProcessorNode (deprecated but Emscripten still uses it) ----
    function StubScriptProcessor(ctx, bufSize) {
        StubAudioNode.call(this);
        this.context        = ctx;
        this.bufferSize     = bufSize || 4096;
        this.numberOfInputs  = 1;
        this.numberOfOutputs = 1;
        this.onaudioprocess = null;
    }
    StubScriptProcessor.prototype = Object.create(StubAudioNode.prototype);

    // ---- AnalyserNode ----
    function StubAnalyserNode(ctx) {
        StubAudioNode.call(this);
        this.context = ctx;
        this.fftSize = 2048;
        this.frequencyBinCount = 1024;
        this.minDecibels = -100;
        this.maxDecibels = -30;
        this.smoothingTimeConstant = 0.8;
    }
    StubAnalyserNode.prototype = Object.create(StubAudioNode.prototype);
    StubAnalyserNode.prototype.getFloatFrequencyData    = function(arr) { if (arr) arr.fill(0); };
    StubAnalyserNode.prototype.getByteFrequencyData     = function(arr) { if (arr) arr.fill(0); };
    StubAnalyserNode.prototype.getFloatTimeDomainData   = function(arr) { if (arr) arr.fill(0); };
    StubAnalyserNode.prototype.getByteTimeDomainData    = function(arr) { if (arr) arr.fill(128); };

    // ---- ChannelMergerNode / ChannelSplitterNode ----
    function StubChannelMergerNode(ctx) {
        StubAudioNode.call(this);
        this.context = ctx;
    }
    StubChannelMergerNode.prototype = Object.create(StubAudioNode.prototype);

    function StubChannelSplitterNode(ctx) {
        StubAudioNode.call(this);
        this.context = ctx;
    }
    StubChannelSplitterNode.prototype = Object.create(StubAudioNode.prototype);

    // ---- AudioBuffer ----
    function StubAudioBuffer(numChannels, length, sampleRate) {
        this.numberOfChannels = numChannels || 2;
        this.length     = length || 0;
        this.sampleRate = sampleRate || 44100;
        this.duration   = this.length > 0 ? this.length / this.sampleRate : 0;
        this._channels  = [];
        for (var i = 0; i < this.numberOfChannels; i++) {
            this._channels.push(new Float32Array(this.length));
        }
    }
    StubAudioBuffer.prototype.getChannelData = function(ch) {
        return this._channels[ch] || new Float32Array(0);
    };
    StubAudioBuffer.prototype.copyFromChannel = function(dest, ch, offset) {
        var src = this.getChannelData(ch);
        var off = offset || 0;
        dest.set(src.subarray(off, off + dest.length));
    };
    StubAudioBuffer.prototype.copyToChannel = function(src, ch, offset) {
        var dest = this.getChannelData(ch);
        var off = offset || 0;
        dest.set(src.subarray(0, Math.min(src.length, dest.length - off)), off);
    };

    // ---- AudioListener ----
    function StubAudioListener() {
        this.positionX = new StubAudioParam(0);
        this.positionY = new StubAudioParam(0);
        this.positionZ = new StubAudioParam(0);
        this.forwardX  = new StubAudioParam(0);
        this.forwardY  = new StubAudioParam(0);
        this.forwardZ  = new StubAudioParam(-1);
        this.upX       = new StubAudioParam(0);
        this.upY       = new StubAudioParam(1);
        this.upZ       = new StubAudioParam(0);
    }
    StubAudioListener.prototype.setPosition    = function() {};
    StubAudioListener.prototype.setOrientation = function() {};

    // ---- AudioContext ----
    function StubAudioContext() {
        this.sampleRate  = 44100;
        this.currentTime = 0;
        this.state       = 'running';
        this.baseLatency = 0.01;
        this.destination = new StubDestination(this);
        this.listener    = new StubAudioListener();
    }
    StubAudioContext.prototype.createGain              = function() { return new StubGainNode(this); };
    StubAudioContext.prototype.createBufferSource       = function() { return new StubBufferSource(this); };
    StubAudioContext.prototype.createOscillator         = function() { return new StubOscillatorNode(this); };
    StubAudioContext.prototype.createAnalyser           = function() { return new StubAnalyserNode(this); };
    StubAudioContext.prototype.createBiquadFilter       = function() { return new StubBiquadFilterNode(this); };
    StubAudioContext.prototype.createDynamicsCompressor = function() { return new StubDynamicsCompressorNode(this); };
    StubAudioContext.prototype.createChannelMerger      = function(n) { return new StubChannelMergerNode(this); };
    StubAudioContext.prototype.createChannelSplitter    = function(n) { return new StubChannelSplitterNode(this); };
    StubAudioContext.prototype.createBuffer = function(ch, len, rate) {
        return new StubAudioBuffer(ch, len, rate);
    };
    StubAudioContext.prototype.createScriptProcessor = function(bufSize, numIn, numOut) {
        return new StubScriptProcessor(this, bufSize || 4096);
    };
    StubAudioContext.prototype.decodeAudioData = function(buf, success, error) {
        var buffer = new StubAudioBuffer(2, 1, 44100);
        if (success) { try { success(buffer); } catch(e) {} }
        return Promise.resolve(buffer);
    };
    StubAudioContext.prototype.resume  = function() { this.state = 'running';   return Promise.resolve(); };
    StubAudioContext.prototype.suspend = function() { this.state = 'suspended'; return Promise.resolve(); };
    StubAudioContext.prototype.close   = function() { this.state = 'closed';    return Promise.resolve(); };

    window.AudioContext       = StubAudioContext;
    window.webkitAudioContext = StubAudioContext;

    // ---- HTMLAudioElement (new Audio()) ----
    if (typeof window.Audio === 'undefined') {
        window.Audio = function(src) {
            this.src         = src || '';
            this.volume      = 1.0;
            this.currentTime = 0;
            this.duration    = 0;
            this.paused      = true;
            this.ended       = false;
            this.muted       = false;
            this.loop        = false;
            this.autoplay    = false;
            this.preload     = 'auto';
            this.readyState  = 0;
            this._listeners  = {};
        };
        window.Audio.prototype.play  = function() { return Promise.resolve(); };
        window.Audio.prototype.pause = function() {};
        window.Audio.prototype.load  = function() {};
        window.Audio.prototype.canPlayType = function() { return ''; };
        window.Audio.prototype.addEventListener = function(evt, fn) {
            if (!this._listeners[evt]) this._listeners[evt] = [];
            this._listeners[evt].push(fn);
        };
        window.Audio.prototype.removeEventListener = function(evt, fn) {
            if (!this._listeners[evt]) return;
            this._listeners[evt] = this._listeners[evt].filter(function(f) { return f !== fn; });
        };
        window.Audio.prototype.cloneNode = function() { return new Audio(this.src); };
    }
})();
)JS";
    }

}  // namespace PrismaUI::Stubs
