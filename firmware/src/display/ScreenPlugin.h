// ScreenPlugin — the modular "app" interface (InkyPi-style plugins).
// The photo frame is ScreenPlugin #1; Clock/Weather/Calendar/News follow. The render
// engine composites whatever the active plugin draws; the PluginScheduler decides which
// plugin is active (see PluginScheduler.h). Local plugins need nothing extra; network
// plugins (Weather/Calendar/News) additionally need the TLS+JSON layer (ADR-009).
#pragma once

namespace lf {

class ICanvas;  // drawing surface abstraction (framebuffer-backed); defined with the renderer.

class ScreenPlugin {
public:
    virtual ~ScreenPlugin() = default;

    // Stable identifier, e.g. "photo", "clock", "weather".
    virtual const char* id() const = 0;

    // Lifecycle: called when this plugin becomes / stops being the active screen.
    virtual void on_activate() {}
    virtual void on_deactivate() {}

    // Background refresh (e.g. fetch weather). Called periodically regardless of whether
    // the plugin is currently on-screen; no-op for purely local plugins. Must not block.
    virtual void update() {}

    // Draw current content onto the canvas. Called each frame while active.
    virtual void render(ICanvas& canvas) = 0;

    // Whether the plugin currently has something worth showing (e.g. weather fetched ok).
    // The scheduler may skip a plugin that returns false.
    virtual bool has_content() const { return true; }

    // If true, the render loop re-renders every tick (e.g. a live clock). If false, the
    // plugin is drawn once on activation (e.g. a static photo) — cheaper.
    virtual bool wants_continuous_redraw() const { return false; }
};

}  // namespace lf
