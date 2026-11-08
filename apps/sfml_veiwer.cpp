// Interactive SFML 2.x map viewer for exploring road networks.
// The viewer owns the camera and controls; MST calculations still use
// the same shared core library as the command-line program.

#include "Analysis.h"
#include "CostScenario.h"
#include "Graph.h"

#include <SFML/Graphics.hpp>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

// Global UI sizing constants. These define the main window layout, map bounds,
// and the zoom limits used by the interactive viewer.
constexpr unsigned int START_WIDTH = 1440;
constexpr unsigned int START_HEIGHT = 900;
constexpr float SIDEBAR_WIDTH = 316.0f;
constexpr float TOPBAR_HEIGHT = 74.0f;
constexpr float FOOTER_HEIGHT = 38.0f;
constexpr float MAP_PADDING = 52.0f;
constexpr float WORLD_SPAN = 1000.0f;
constexpr float MIN_ZOOM = 0.12f;
constexpr float MAX_ZOOM = 35.0f;
constexpr double PI = 3.14159265358979323846;

// Shared color palette for the map, side panel, and status overlays.
const sf::Color BACKGROUND(12, 19, 32);
const sf::Color PANEL(20, 30, 46);
const sf::Color PANEL_LIGHT(30, 44, 63);
const sf::Color MAP_BACKGROUND(14, 29, 44);
const sf::Color TEXT(232, 240, 249);
const sf::Color MUTED(145, 165, 187);
const sf::Color ACCENT(62, 211, 159);
const sf::Color BLUE(81, 157, 241);
const sf::Color ROAD_COLOR(68, 95, 118, 170);
const sf::Color MST_COLOR(81, 231, 173);
const sf::Color DANGER(245, 112, 114);

// Each button corresponds to a single action in the GUI, such as switching
// algorithms or changing the cost scenario.
struct Button {
    sf::FloatRect bounds;
    std::string label;
    int action;
    bool active = false;
};

enum Action {
    PRIM, KRUSKAL, LENGTH, FIBER, FLOOD, FIT, ZOOM_IN, ZOOM_OUT,
    HUB, RESET
};

// Generic helper to keep values inside a reasonable range while drawing or zooming.
float clampFloat(float value, float low, float high) {
    return std::max(low, std::min(high, value));
}

// Fast squared distance check for comparing map points without taking a square root.
float squaredDistance(sf::Vector2f a, sf::Vector2f b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    return dx * dx + dy * dy;
}

// Distance from a point to a line segment. This is used to detect which road is
// nearest when the user clicks on the map while closing roads.
float distanceToSegmentSquared(sf::Vector2f point, sf::Vector2f a,
                               sf::Vector2f b) {
    const float dx = b.x - a.x;
    const float dy = b.y - a.y;
    const float lengthSquared = dx * dx + dy * dy;
    const float t = lengthSquared > 0.0f
        ? clampFloat(((point.x - a.x) * dx + (point.y - a.y) * dy) /
                         lengthSquared, 0.0f, 1.0f)
        : 0.0f;
    return squaredDistance(point, {a.x + t * dx, a.y + t * dy});
}

// Convert floating-point values into compact strings for UI labels.
std::string formatNumber(double value, int precision = 0) {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(precision) << value;
    return stream.str();
}

// Reduce large cost or time numbers into easy-to-read compact quantities.
std::string shortNumber(double value) {
    if (std::abs(value) >= 1000000.0) return formatNumber(value / 1000000.0, 2) + "M";
    if (std::abs(value) >= 1000.0) return formatNumber(value / 1000.0, 1) + "K";
    return formatNumber(value, 0);
}

// Main viewer class. It owns the open window, the map camera, the graph data,
// and the current MST configuration for interactive exploration.
class MapViewer {
public:
    // Constructor reads the road graph and its vertex coordinates, initializes the
    // window, and prepares the camera so the map is ready to draw immediately.
    MapViewer(const std::string& graphFile, const std::string& verticesFile)
        : window_(sf::VideoMode(START_WIDTH, START_HEIGHT), "MapaGator | Network Explorer",
                  sf::Style::Default),
          roadLines_(sf::Lines), selectedLines_(sf::Lines),
          closedLines_(sf::Lines), hubLines_(sf::Lines) {
        original_.readFile(graphFile);
        original_.readVertices(verticesFile);
        loadFont();
        prepareCoordinates();
        window_.setFramerateLimit(60);
        window_.setVerticalSyncEnabled(true);
        updateLayout();
        fitMap();
        recompute();
    }

    // Main tick loop: handle user input, refresh the graph if needed, and redraw the UI.
    void run() {
        while (window_.isOpen()) {
            processEvents();
            if (needsUpdate_) recompute();
            render();
        }
    }

private:
    // This section holds the live graph state and the camera/rendering state.
    sf::RenderWindow window_;
    sf::Font font_;
    bool fontLoaded_ = false;

    Graph original_;
    Graph weighted_;
    MSTResult result_;
    NetworkOptions options_;
    CostScenario scenario_ = CostScenario::Length;
    bool usePrim_ = true;
    bool needsUpdate_ = false;

    // Geographic positions are converted once to a consistent world space.
    std::vector<sf::Vector2f> positions_;
    std::vector<bool> hasPosition_;
    sf::FloatRect worldBounds_;
    sf::View mapView_;
    sf::View uiView_;
    sf::FloatRect mapPixels_;
    sf::VertexArray roadLines_;
    sf::VertexArray selectedLines_;
    sf::VertexArray closedLines_;
    sf::VertexArray hubLines_;
    std::vector<Button> buttons_;

    float zoom_ = 1.0f;
    float fitHeight_ = 1000.0f;
    bool dragging_ = false;
    bool dragMoved_ = false;
    sf::Vector2i previousMouse_;
    sf::Vector2i dragOrigin_;
    bool hubMode_ = false;
    int firstHubVertex_ = -1;
    std::string status_ = "Ready. Scroll to zoom, drag to explore.";

    void loadFont() {
        // Try a few standard system fonts so the interface remains usable without
        // shipping extra resources with the project.
        const std::vector<std::string> candidates = {
            "C:/Windows/Fonts/segoeui.ttf",
            "C:/Windows/Fonts/arial.ttf",
            "C:/Windows/Fonts/calibri.ttf",
            "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"
        };
        for (const std::string& filename : candidates) {
            if (font_.loadFromFile(filename)) {
                fontLoaded_ = true;
                return;
            }
        }
        std::cerr << "Warning: UI font unavailable; map will still render.\n";
    }

    // Convert GPS coordinates to a flat space that SFML can draw.
    // Keep this separate from zoom/pan so camera movement stays cheap.
    void prepareCoordinates() {
        const auto& vertices = original_.getVertices();
        positions_.resize(vertices.size());
        hasPosition_.assign(vertices.size(), false);

        double minLat = std::numeric_limits<double>::infinity();
        double maxLat = -minLat;
        double minLon = minLat;
        double maxLon = -minLat;

        for (const Vertex& vertex : vertices) {
            if (!vertex.hasCoordinates) continue;
            minLat = std::min(minLat, vertex.latitude);
            maxLat = std::max(maxLat, vertex.latitude);
            minLon = std::min(minLon, vertex.longitude);
            maxLon = std::max(maxLon, vertex.longitude);
        }
        if (!std::isfinite(minLat)) {
            throw std::runtime_error("Map requires vertex coordinates (vertices.txt)");
        }

        // Approximate east/west scaling so Gainesville does not appear stretched.
        const double longitudeScale = std::max(0.1, std::cos((minLat + maxLat) * PI / 360.0));
        const double width = std::max(1e-12, (maxLon - minLon) * longitudeScale);
        const double height = std::max(1e-12, maxLat - minLat);
        const double worldScale = WORLD_SPAN / std::max(width, height);
        const float worldWidth = static_cast<float>(width * worldScale);
        const float worldHeight = static_cast<float>(height * worldScale);
        worldBounds_ = {0.0f, 0.0f, worldWidth, worldHeight};

        for (const Vertex& vertex : vertices) {
            if (!vertex.hasCoordinates) continue;
            const auto index = static_cast<std::size_t>(vertex.id);
            positions_[index] = {
                static_cast<float>((vertex.longitude - minLon) * longitudeScale * worldScale),
                static_cast<float>((maxLat - vertex.latitude) * worldScale)
            };
            hasPosition_[index] = true;
        }
    }

    // Check whether a vertex has valid map coordinates. Some vertices may exist
    // only as abstract graph nodes without GPS data.
    bool validPosition(int vertex) const {
        return vertex >= 0 && static_cast<std::size_t>(vertex) < hasPosition_.size()
            && hasPosition_[static_cast<std::size_t>(vertex)];
    }

    // Add a line segment for a graph edge using the precomputed coordinate mapping.
    void addLine(sf::VertexArray& lines, int from, int to, sf::Color color) const {
        if (!validPosition(from) || !validPosition(to)) return;
        lines.append(sf::Vertex(positions_[from], color));
        lines.append(sf::Vertex(positions_[to], color));
    }

    // Recalculate the viewports and sidebar geometry after a resize or startup.
    void updateLayout() {
        const sf::Vector2u size = window_.getSize();
        const float width = static_cast<float>(size.x);
        const float height = static_cast<float>(size.y);
        uiView_.reset(sf::FloatRect(0.0f, 0.0f, width, height));
        mapPixels_ = {
            SIDEBAR_WIDTH,
            TOPBAR_HEIGHT,
            std::max(100.0f, width - SIDEBAR_WIDTH),
            std::max(100.0f, height - TOPBAR_HEIGHT - FOOTER_HEIGHT)
        };
        mapView_.setViewport({
            mapPixels_.left / width, mapPixels_.top / height,
            mapPixels_.width / width, mapPixels_.height / height
        });
        makeButtons();
        refreshCameraSize();
    }

    void refreshCameraSize() {
        // Preserve zoom during window resizing; only the aspect ratio changes.
        const float aspect = mapPixels_.width / mapPixels_.height;
        const float visibleHeight = fitHeight_ / zoom_;
        mapView_.setSize(visibleHeight * aspect, visibleHeight);
    }

    // Reset the camera to show the entire loaded road network.
    void fitMap() {
        const float aspect = mapPixels_.width / mapPixels_.height;
        fitHeight_ = std::max(worldBounds_.height + 2.0f * MAP_PADDING,
                              (worldBounds_.width + 2.0f * MAP_PADDING) / aspect);
        zoom_ = 1.0f;
        mapView_.setCenter(worldBounds_.left + worldBounds_.width / 2.0f,
                           worldBounds_.top + worldBounds_.height / 2.0f);
        refreshCameraSize();
        status_ = "Map fitted to window.";
    }

    // Determine whether a screen coordinate lies within the interactive map region.
    bool insideMap(sf::Vector2i pixel) const {
        return mapPixels_.contains(static_cast<float>(pixel.x),
                                   static_cast<float>(pixel.y));
    }

    // Zoom around the cursor instead of jumping toward the window center.
    void zoomAt(float factor, sf::Vector2i pixel) {
        if (!insideMap(pixel)) return;
        const float newZoom = clampFloat(zoom_ * factor, MIN_ZOOM, MAX_ZOOM);
        if (newZoom == zoom_) return;
        const sf::Vector2f before = window_.mapPixelToCoords(pixel, mapView_);
        zoom_ = newZoom;
        refreshCameraSize();
        const sf::Vector2f after = window_.mapPixelToCoords(pixel, mapView_);
        mapView_.move(before - after); // Keep the point under the cursor fixed.
    }

    // Move the camera using a mouse drag.
    void pan(sf::Vector2i previous, sf::Vector2i current) {
        const sf::Vector2f before = window_.mapPixelToCoords(previous, mapView_);
        const sf::Vector2f after = window_.mapPixelToCoords(current, mapView_);
        mapView_.move(before - after);
    }

    // Convert a screen-space radius into world-space units so markers remain
    // readable after zooming in or out.
    float pickRadius(float pixelRadius) const {
        return mapView_.getSize().x * pixelRadius / mapPixels_.width;
    }

    // Find the closest intersection to the cursor for anchor or hub operations.
    int nearestVertex(sf::Vector2f point) const {
        const float radius = pickRadius(16.0f);
        float best = radius * radius;
        int closest = -1;
        for (std::size_t index = 0; index < positions_.size(); ++index) {
            if (!hasPosition_[index]) continue;
            const float distance = squaredDistance(point, positions_[index]);
            if (distance < best) {
                best = distance;
                closest = static_cast<int>(index);
            }
        }
        return closest;
    }

    // Find the road under the pointer for Shift-click road closures.
    int nearestRoad(sf::Vector2f point) const {
        const float radius = pickRadius(13.0f);
        float best = radius * radius;
        int closest = -1;
        const auto& roads = original_.getEdges();
        for (std::size_t i = 0; i < roads.size(); ++i) {
            const Edge& road = roads[i];
            if (!validPosition(road.source) || !validPosition(road.destination)) continue;
            const float distance = distanceToSegmentSquared(
                point, positions_[road.source], positions_[road.destination]);
            if (distance < best) {
                best = distance;
                closest = static_cast<int>(i);
            }
        }
        return closest;
    }

    // Closing a road adds it to the network options so the MST is recomputed.
    void toggleRoad(int roadIndex) {
        auto& closed = options_.closedEdges;
        const auto it = std::find(closed.begin(), closed.end(),
                                  static_cast<std::size_t>(roadIndex));
        if (it == closed.end()) {
            closed.push_back(static_cast<std::size_t>(roadIndex));
            status_ = "Road closed. MST recalculated.";
        } else {
            closed.erase(it);
            status_ = "Road reopened. MST recalculated.";
        }
        needsUpdate_ = true;
    }

    // Toggle the zero-cost anchor on a selected vertex. This affects the MST by
    // connecting anchors with an artificial zero-cost link.
    void toggleAnchor(int vertex) {
        auto& anchors = options_.anchors;
        const auto it = std::find(anchors.begin(), anchors.end(), vertex);
        if (it == anchors.end()) {
            anchors.push_back(vertex);
            status_ = "Anchor added (virtual zero-cost connection).";
        } else {
            anchors.erase(it);
            status_ = "Anchor removed.";
        }
        needsUpdate_ = true;
    }

    // Create a hypothetical road between two clicked intersections and store it
    // in the working graph options for the next MST calculation.
    void createHub(int vertex) {
        if (firstHubVertex_ < 0) {
            firstHubVertex_ = vertex;
            status_ = "Hub: select the second endpoint.";
            return;
        }
        if (firstHubVertex_ == vertex) return;

        const Vertex& a = original_.getVertices()[firstHubVertex_];
        const Vertex& b = original_.getVertices()[vertex];
        const double northSouth = (a.latitude - b.latitude) * 111132.0;
        const double eastWest = (a.longitude - b.longitude) * 111320.0
            * std::cos((a.latitude + b.latitude) * PI / 360.0);
        const double meters = std::hypot(northSouth, eastWest);

        // Hypothetical straight-line connection, not an actual road route.
        options_.hubEdges.emplace_back(firstHubVertex_, vertex, meters);
        firstHubVertex_ = -1;
        hubMode_ = false;
        status_ = "New hypothetical road added.";
        needsUpdate_ = true;
    }

    // Route map clicks to road closures, anchors, or new hub connections.
    void mapClick(sf::Vector2i pixel, bool rightButton, bool shiftHeld) {
        const sf::Vector2f point = window_.mapPixelToCoords(pixel, mapView_);
        if (hubMode_ || rightButton) {
            const int vertex = nearestVertex(point);
            if (vertex < 0) {
                status_ = "Zoom in and click closer to an intersection.";
                return;
            }
            if (hubMode_) createHub(vertex);
            else toggleAnchor(vertex);
        } else if (shiftHeld) {
            const int road = nearestRoad(point);
            if (road >= 0) toggleRoad(road);
            else status_ = "No road close enough. Zoom in and try again.";
        }
    }

    // Convert a GUI action into the appropriate state change.
    void performAction(int action) {
        switch (action) {
            case PRIM: usePrim_ = true; needsUpdate_ = true; break;
            case KRUSKAL: usePrim_ = false; needsUpdate_ = true; break;
            case LENGTH: scenario_ = CostScenario::Length; needsUpdate_ = true; break;
            case FIBER: scenario_ = CostScenario::Fiber; needsUpdate_ = true; break;
            case FLOOD: scenario_ = CostScenario::Flood; needsUpdate_ = true; break;
            case FIT: fitMap(); break;
            case ZOOM_IN:
            case ZOOM_OUT: {
                sf::Vector2i center(static_cast<int>(mapPixels_.left + mapPixels_.width / 2),
                                    static_cast<int>(mapPixels_.top + mapPixels_.height / 2));
                zoomAt(action == ZOOM_IN ? 1.35f : 1.0f / 1.35f, center);
                break;
            }
            case HUB:
                hubMode_ = !hubMode_;
                firstHubVertex_ = -1;
                status_ = hubMode_ ? "Hub mode: click two intersections." : "Hub mode canceled.";
                break;
            case RESET:
                options_ = NetworkOptions{};
                hubMode_ = false;
                firstHubVertex_ = -1;
                status_ = "All road edits and anchors reset.";
                needsUpdate_ = true;
                break;
        }
    }

    // Keyboard shortcuts permit quick changes without using the sidebar buttons.
    void handleKey(sf::Keyboard::Key key) {
        switch (key) {
            case sf::Keyboard::Escape: window_.close(); break;
            case sf::Keyboard::P: performAction(PRIM); break;
            case sf::Keyboard::K: performAction(KRUSKAL); break;
            case sf::Keyboard::Num1: performAction(LENGTH); break;
            case sf::Keyboard::Num2: performAction(FIBER); break;
            case sf::Keyboard::Num3: performAction(FLOOD); break;
            case sf::Keyboard::F: performAction(FIT); break;
            case sf::Keyboard::H: performAction(HUB); break;
            case sf::Keyboard::R: performAction(RESET); break;
            case sf::Keyboard::Add:
            case sf::Keyboard::Equal: performAction(ZOOM_IN); break;
            case sf::Keyboard::Subtract:
            case sf::Keyboard::Dash: performAction(ZOOM_OUT); break;
            default: break;
        }
    }

    // Handle mouse and keyboard input before drawing the next frame.
    void processEvents() {
        sf::Event event;
        while (window_.pollEvent(event)) {
            if (event.type == sf::Event::Closed) window_.close();
            else if (event.type == sf::Event::Resized) {
                updateLayout();
            } else if (event.type == sf::Event::KeyPressed) {
                handleKey(event.key.code);
            } else if (event.type == sf::Event::MouseWheelScrolled) {
                if (event.mouseWheelScroll.wheel == sf::Mouse::VerticalWheel) {
                    zoomAt(std::pow(1.20f, event.mouseWheelScroll.delta),
                           {event.mouseWheelScroll.x, event.mouseWheelScroll.y});
                }
            } else if (event.type == sf::Event::MouseButtonPressed) {
                const sf::Vector2i pixel(event.mouseButton.x, event.mouseButton.y);
                if (event.mouseButton.button == sf::Mouse::Left) {
                    bool clickedButton = false;
                    for (const Button& button : buttons_) {
                        if (button.bounds.contains(static_cast<float>(pixel.x),
                                                   static_cast<float>(pixel.y))) {
                            performAction(button.action);
                            clickedButton = true;
                            break;
                        }
                    }
                    if (!clickedButton && insideMap(pixel)) {
                        dragging_ = true;
                        dragMoved_ = false;
                        previousMouse_ = dragOrigin_ = pixel;
                    }
                } else if (event.mouseButton.button == sf::Mouse::Right && insideMap(pixel)) {
                    mapClick(pixel, true, false);
                }
            } else if (event.type == sf::Event::MouseMoved && dragging_) {
                const sf::Vector2i pixel(event.mouseMove.x, event.mouseMove.y);
                if (squaredDistance({static_cast<float>(pixel.x), static_cast<float>(pixel.y)},
                                    {static_cast<float>(dragOrigin_.x),
                                     static_cast<float>(dragOrigin_.y)}) > 16.0f) {
                    dragMoved_ = true;
                }
                if (dragMoved_) pan(previousMouse_, pixel);
                previousMouse_ = pixel;
            } else if (event.type == sf::Event::MouseButtonReleased &&
                       event.mouseButton.button == sf::Mouse::Left && dragging_) {
                dragging_ = false;
                if (!dragMoved_) {
                    const sf::Vector2i pixel(event.mouseButton.x, event.mouseButton.y);
                    if (insideMap(pixel)) {
                        mapClick(pixel, false,
                            sf::Keyboard::isKeyPressed(sf::Keyboard::LShift) ||
                            sf::Keyboard::isKeyPressed(sf::Keyboard::RShift));
                    }
                }
            }
        }
    }

    // Only recompute the MST when network options or weights change.
    void recompute() {
        weighted_ = applyScenario(original_, scenario_);
        result_ = analyze(weighted_, usePrim_, options_);
        rebuildLines();
        needsUpdate_ = false;
        std::cout << (usePrim_ ? "Prim" : "Kruskal")
                  << " | cost " << result_.totalCost
                  << " | selected roads " << result_.edges.size()
                  << " | " << result_.runtimeMs << " ms\n";
    }

    // Cache geometry for the roads so drawing does not rebuild the graph.
    void rebuildLines() {
        roadLines_.clear();
        selectedLines_.clear();
        closedLines_.clear();
        hubLines_.clear();

        // Keep base roads in one static vertex buffer; zooming never recomputes the MST.
        const auto& edges = original_.getEdges();
        std::vector<bool> closed(edges.size(), false);
        for (std::size_t index : options_.closedEdges) {
            if (index < closed.size()) closed[index] = true;
        }
        for (std::size_t index = 0; index < edges.size(); ++index) {
            const Edge& edge = edges[index];
            if (closed[index]) addLine(closedLines_, edge.source, edge.destination, DANGER);
            else addLine(roadLines_, edge.source, edge.destination, ROAD_COLOR);
        }
        for (const Edge& edge : options_.hubEdges) {
            addLine(hubLines_, edge.source, edge.destination, BLUE);
        }
        for (const Edge& edge : result_.edges) {
            // Results can include virtual anchor vertices; those are not real roads.
            addLine(selectedLines_, edge.source, edge.destination, MST_COLOR);
        }
    }

    // Build the sidebar buttons from the fixed layout so we can redraw them
    // whenever the window is resized.
    void makeButtons() {
        buttons_.clear();
        auto add = [this](float x, float y, float w, float h,
                          const std::string& label, int action) {
            buttons_.push_back({{x, y, w, h}, label, action, false});
        };
        add(20, 177, 132, 40, "Prim's", PRIM);
        add(164, 177, 132, 40, "Kruskal's", KRUSKAL);
        add(20, 284, 276, 38, "1   Road length", LENGTH);
        add(20, 329, 276, 38, "2   Fiber installation", FIBER);
        add(20, 374, 276, 38, "3   Flood risk", FLOOD);
        add(20, 490, 132, 38, "Fit map  [F]", FIT);
        add(164, 490, 62, 38, "+", ZOOM_IN);
        add(234, 490, 62, 38, "-", ZOOM_OUT);
        add(20, 610, 276, 38, "Add hub [H]", HUB);
        add(20, 660, 276, 38, "Reset edits [R]", RESET);
    }

    // Small drawing helpers keep the render code cleaner and ensure consistent
    // panel styling across the interface.
    void rectangle(sf::FloatRect rect, sf::Color color,
                   float outline = 0.0f, sf::Color outlineColor = sf::Color::Transparent) {
        sf::RectangleShape shape({rect.width, rect.height});
        shape.setPosition(rect.left, rect.top);
        shape.setFillColor(color);
        shape.setOutlineThickness(outline);
        shape.setOutlineColor(outlineColor);
        window_.draw(shape);
    }

    void label(const std::string& words, float x, float y, unsigned int size,
               sf::Color color = TEXT, bool bold = false) {
        if (!fontLoaded_) return;
        sf::Text text(words, font_, size);
        text.setFillColor(color);
        text.setPosition(x, y);
        if (bold) text.setStyle(sf::Text::Bold);
        window_.draw(text);
    }

    void circle(sf::Vector2f position, float radius, sf::Color fill,
                float border = 0.0f, sf::Color borderColor = sf::Color::Transparent) {
        sf::CircleShape shape(radius);
        shape.setOrigin(radius, radius);
        shape.setPosition(position);
        shape.setFillColor(fill);
        shape.setOutlineThickness(border);
        shape.setOutlineColor(borderColor);
        window_.draw(shape);
    }

    // Render the background network first, then draw selected roads on top.
    void drawMap() {
        window_.setView(mapView_);
        window_.draw(roadLines_);
        window_.draw(selectedLines_);
        window_.draw(hubLines_);
        window_.draw(closedLines_);

        // Marker size is based on camera scale so anchor dots remain readable.
        const float radius = pickRadius(5.0f);
        for (int vertex : options_.anchors) {
            if (validPosition(vertex)) {
                circle(positions_[vertex], radius, BLUE, radius * 0.35f, MAP_BACKGROUND);
            }
        }
        if (validPosition(firstHubVertex_)) {
            circle(positions_[firstHubVertex_], radius * 1.5f, BLUE);
        }
    }

    // Draw the interactive buttons and highlight the currently active algorithm mode.
    void drawButtons() {
        for (const Button& button : buttons_) {
            bool active = false;
            switch (button.action) {
                case PRIM: active = usePrim_; break;
                case KRUSKAL: active = !usePrim_; break;
                case LENGTH: active = scenario_ == CostScenario::Length; break;
                case FIBER: active = scenario_ == CostScenario::Fiber; break;
                case FLOOD: active = scenario_ == CostScenario::Flood; break;
                case HUB: active = hubMode_; break;
                default: break;
            }
            const auto mouse = sf::Mouse::getPosition(window_);
            const bool hovered = button.bounds.contains(
                static_cast<float>(mouse.x), static_cast<float>(mouse.y));
            rectangle(button.bounds, active ? sf::Color(33, 92, 83)
                                           : hovered ? sf::Color(45, 63, 84) : PANEL_LIGHT,
                      active ? 1.0f : 0.0f, ACCENT);
            label(button.label, button.bounds.left + 12,
                  button.bounds.top + 8, 15, active ? ACCENT : TEXT, active);
        }
    }

    // Draw the algorithm/scenario controls and the current MST statistics.
    void drawSidebar() {
        const float height = static_cast<float>(window_.getSize().y);
        rectangle({0, TOPBAR_HEIGHT, SIDEBAR_WIDTH, height - TOPBAR_HEIGHT}, PANEL);
        rectangle({SIDEBAR_WIDTH - 1, TOPBAR_HEIGHT, 1, height - TOPBAR_HEIGHT}, PANEL_LIGHT);

        label("ALGORITHM", 20, 139, 13, MUTED, true);
        label("COST SCENARIO", 20, 245, 13, MUTED, true);
        label("MAP NAVIGATION", 20, 452, 13, MUTED, true);
        label("NETWORK EDITS", 20, 573, 13, MUTED, true);
        drawButtons();

        const float statsTop = std::max(723.0f, height - 175.0f);
        if (height > statsTop + 100.0f) {
            rectangle({20, statsTop, 276, std::min(138.0f, height - statsTop - 10)},
                      BACKGROUND);
            label("CURRENT SELECTION", 33, statsTop + 11, 12, MUTED, true);
            label("Anchors: " + std::to_string(options_.anchors.size()),
                  33, statsTop + 39, 14);
            label("Closed roads: " + std::to_string(options_.closedEdges.size()),
                  33, statsTop + 67, 14);
            label("Added links: " + std::to_string(options_.hubEdges.size()),
                  33, statsTop + 95, 14);
        }
    }

    // Top bar with the title and the live network summary for the loaded map.
    void drawHeader() {
        const float width = static_cast<float>(window_.getSize().x);
        rectangle({0, 0, width, TOPBAR_HEIGHT}, BACKGROUND);
        rectangle({18, 18, 38, 38}, sf::Color(31, 104, 88));
        label("M", 27, 19, 27, ACCENT, true);
        label("MapaGator", 68, 14, 24, TEXT, true);
        label("ROAD NETWORK EXPLORER", 70, 47, 10, MUTED, true);
        label("INTERACTIVE MST ANALYSIS", SIDEBAR_WIDTH + 26, 19, 14, MUTED, true);
        label(std::to_string(original_.getNumVertices()) + " intersections  /  " +
              std::to_string(original_.getEdges().size()) + " road segments",
              SIDEBAR_WIDTH + 26, 42, 13, TEXT);
    }

    // Floating summary cards and a small legend overlay help explain the map state
    // without requiring the user to open the terminal.
    void drawMapOverlay() {
        const float right = static_cast<float>(window_.getSize().x);
        const float bottom = static_cast<float>(window_.getSize().y);
        const float x = SIDEBAR_WIDTH + 24.0f;
        const float y = TOPBAR_HEIGHT + 20.0f;
        const float available = right - x - 30.0f;

        // Compact summary cards on top of the map.
        const float cardWidth = std::min(210.0f, std::max(135.0f, (available - 36.0f) / 4.0f));
        const float cardGap = 12.0f;
        const std::vector<std::pair<std::string, std::string>> stats = {
            {"TOTAL MST COST", shortNumber(result_.totalCost)},
            {"SELECTED ROADS", shortNumber(static_cast<double>(result_.edges.size()))},
            {"COMPUTE TIME", formatNumber(result_.runtimeMs, 2) + " ms"},
            {"MST MILEAGE", formatNumber(result_.totalLengthMeters / 1609.344, 1) + " mi"}
        };
        for (std::size_t i = 0; i < stats.size(); ++i) {
            const float left = x + static_cast<float>(i) * (cardWidth + cardGap);
            if (left + cardWidth > right - 12) break;
            rectangle({left, y, cardWidth, 76}, sf::Color(23, 39, 56, 240));
            label(stats[i].first, left + 13, y + 12, 11, MUTED, true);
            label(stats[i].second, left + 13, y + 35, 22, TEXT, true);
        }

        // Legend and live instructions remain visible without using a terminal.
        const float legendY = bottom - FOOTER_HEIGHT - 78.0f;
        rectangle({x, legendY, std::min(500.0f, available), 56},
                  sf::Color(23, 39, 56, 230));
        rectangle({x + 13, legendY + 17, 23, 3}, MST_COLOR);
        label("MST", x + 43, legendY + 10, 13);
        rectangle({x + 110, legendY + 17, 23, 3}, ROAD_COLOR);
        label("Roads", x + 140, legendY + 10, 13);
        rectangle({x + 225, legendY + 17, 23, 3}, DANGER);
        label("Closed", x + 255, legendY + 10, 13);
        label("Scroll: zoom   |   Drag: pan   |   Shift+click: close road   |   Right click: anchor",
              x + 13, legendY + 34, 11, MUTED);

        // Zoom percentage is relative to the initial fitted map.
        const std::string zoomText = "ZOOM " + formatNumber(zoom_ * 100.0f) + "%";
        label(zoomText, right - 130, bottom - FOOTER_HEIGHT - 35, 13, MUTED, true);
    }

    // Footer status bar reports the current mode and the latest user action.
    void drawFooter() {
        const float width = static_cast<float>(window_.getSize().x);
        const float height = static_cast<float>(window_.getSize().y);
        rectangle({0, height - FOOTER_HEIGHT, width, FOOTER_HEIGHT}, BACKGROUND);
        circle({21, height - 18}, 5.0f, ACCENT);
        label(status_, 34, height - 28, 13, TEXT);
        const std::string mode = std::string(usePrim_ ? "PRIM" : "KRUSKAL") +
            "  /  " + (scenario_ == CostScenario::Length ? "LENGTH" :
                         scenario_ == CostScenario::Fiber ? "FIBER" : "FLOOD");
        label(mode, std::max(350.0f, width - 235.0f), height - 28, 12, MUTED, true);
    }

    // Full render pass: draw the world map, then overlay the controls and status text.
    void render() {
        window_.clear(MAP_BACKGROUND);
        drawMap();
        window_.setView(uiView_);
        drawHeader();
        drawSidebar();
        drawMapOverlay();
        drawFooter();
        window_.display();
    }
};

} // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: MapaGatorGUI graph_data.txt vertices.txt\n";
        return 2;
    }
    try {
        MapViewer viewer(argv[1], argv[2]);
        viewer.run();
    } catch (const std::exception& error) {
        std::cerr << "MapaGatorGUI error: " << error.what() << '\n';
        return 1;
    }
}
