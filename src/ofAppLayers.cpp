#include "ofApp.h"
#include <sstream>

//--------------------------------------------------------------
ofApp::Layer &ofApp::activeLayer(){
	return *layers[activeLayerIndex];
}

//--------------------------------------------------------------
ofVec2f ofApp::canvasPointToLayer(const Layer &layer, const ofVec2f &point) const{
	return ofVec2f((point.x - layer.transformPosition.x) / layer.transformScale,
		(point.y - layer.transformPosition.y) / layer.transformScale);
}

//--------------------------------------------------------------
ofVec2f ofApp::screenToCanvasPoint(int x, int y) const{
	return ofVec2f((x - canvasBounds.x) / canvasViewScale,
		(y - canvasBounds.y) / canvasViewScale);
}

//--------------------------------------------------------------
ofRectangle ofApp::strokeContentBounds(const Stroke &stroke) const{
	bool hasBounds = false;
	float minX = 0.0f;
	float minY = 0.0f;
	float maxX = 0.0f;
	float maxY = 0.0f;
	auto includePoint = [&](float x, float y){
		if (!hasBounds) {
			minX = maxX = x;
			minY = maxY = y;
			hasBounds = true;
			return;
		}
		minX = std::min(minX, x);
		minY = std::min(minY, y);
		maxX = std::max(maxX, x);
		maxY = std::max(maxY, y);
	};
	if (!stroke.fillSpans.empty()) {
		for (const auto &span : stroke.fillSpans) {
			includePoint(span.x, span.y);
			includePoint(span.x + span.width, span.y + 1.0f);
		}
	} else {
		const float radius = std::max(1.0f, stroke.width * 0.5f);
		for (const auto &point : stroke.points) {
			includePoint(point.x - radius, point.y - radius);
			includePoint(point.x + radius, point.y + radius);
		}
	}
	if (!hasBounds) {
		return ofRectangle();
	}
	return ofRectangle(minX, minY, maxX - minX, maxY - minY);
}

//--------------------------------------------------------------
void ofApp::newProject(){
	std::string dimensions = ofSystemTextBoxDialog(
		"New project canvas size in pixels (width x height). Current artwork will be cleared.",
		"1200x800");
	if (dimensions.empty()) {
		return;
	}
	for (char &character : dimensions) {
		if (character == 'x' || character == 'X' || character == ',') {
			character = ' ';
		}
	}
	std::istringstream input(dimensions);
	int width = 0;
	int height = 0;
	char trailing = 0;
	if (!(input >> width >> height) || (input >> trailing) ||
		width < 64 || height < 64 || width > 4096 || height > 4096) {
		ofSystemAlertDialog("Canvas width and height must each be between 64 and 4096 pixels.");
		return;
	}
	canvasWidth = width;
	canvasHeight = height;
	layers.clear();
	activeLayerIndex = 0;
	selectedLayerItemType = LayerItemType::Layer;
	nextLayerNumber = 0;
	drawingStroke = false;
	pendingStroke = false;
	activeStroke.points.clear();
	activeStroke.pressures.clear();
	activeStroke.fillSpans.clear();
	selectTool(Tool::Brush);
	resizeCanvas(ofGetWidth(), ofGetHeight());
	addLayer();
}

//--------------------------------------------------------------
ofRectangle ofApp::layerContentBounds(const Layer &layer) const{
	bool hasBounds = false;
	float minX = 0.0f;
	float minY = 0.0f;
	float maxX = 0.0f;
	float maxY = 0.0f;
	auto includePoint = [&](float x, float y){
		if (!hasBounds) {
			minX = maxX = x;
			minY = maxY = y;
		hasBounds = true;
			return;
		}
		minX = std::min(minX, x);
		minY = std::min(minY, y);
		maxX = std::max(maxX, x);
		maxY = std::max(maxY, y);
	};
	if (layer.hasImage) {
		const float width = layer.layerImage.getWidth() * layer.imageScale;
		const float height = layer.layerImage.getHeight() * layer.imageScale;
		includePoint(layer.imagePosition.x, layer.imagePosition.y);
		includePoint(layer.imagePosition.x + width, layer.imagePosition.y + height);
	}
	for (const auto &stroke : layer.strokes) {
		const ofRectangle bounds = strokeContentBounds(stroke);
		if (bounds.width > 0.0f && bounds.height > 0.0f) {
			includePoint(bounds.x, bounds.y);
			includePoint(bounds.x + bounds.width, bounds.y + bounds.height);
		}
	}
	return hasBounds ? ofRectangle(minX, minY, maxX - minX, maxY - minY) : ofRectangle();
}

//--------------------------------------------------------------
ofRectangle ofApp::selectedContentBounds(const Layer &layer) const{
	if (selectedLayerItemType == LayerItemType::Image) {
		if (!layer.hasImage) {
			return ofRectangle();
		}
		return ofRectangle(layer.imagePosition.x, layer.imagePosition.y,
			layer.layerImage.getWidth() * layer.imageScale,
			layer.layerImage.getHeight() * layer.imageScale);
	}
	return layerContentBounds(layer);
}

//--------------------------------------------------------------
void ofApp::addLayer(){
	auto layer = std::make_unique<Layer>();
	layer->name = "Layer " + ofToString(++nextLayerNumber);
	ofFbo::Settings settings;
	settings.width = canvasWidth;
	settings.height = canvasHeight;
	settings.internalformat = GL_RGBA;
	settings.useDepth = false;
	settings.useStencil = false;
	layer->image.allocate(settings);
	layer->image.begin();
	ofClear(0, 0, 0, 0);
	layer->image.end();
	layers.push_back(std::move(layer));
	activeLayerIndex = layers.size() - 1;
	selectedLayerItemType = LayerItemType::Layer;
	rebuildCanvas();
	updateLayerPanelLayout();
}

//--------------------------------------------------------------
void ofApp::removeActiveLayer(){
	if (layers.size() <= 1) {
	clearCanvas();
		return;
	}
	layers.erase(layers.begin() + activeLayerIndex);
	activeLayerIndex = std::min(activeLayerIndex, layers.size() - 1);
	selectedLayerItemType = LayerItemType::Layer;
	rebuildCanvas();
	updateLayerPanelLayout();
}

//--------------------------------------------------------------
void ofApp::moveActiveLayer(int direction){
	const int destination = static_cast<int>(activeLayerIndex) + direction;
	if (destination < 0 || destination >= static_cast<int>(layers.size())) {
		return;
	}
	std::swap(layers[activeLayerIndex], layers[destination]);
	activeLayerIndex = static_cast<std::size_t>(destination);
	rebuildCanvas();
	updateLayerPanelLayout();
}

//--------------------------------------------------------------
void ofApp::undoLastStroke(){
	Layer &layer = activeLayer();
	if (!layer.strokes.empty()) {
	layer.strokes.pop_back();
	selectedLayerItemType = LayerItemType::Layer;
	rebuildLayer(layer);
	rebuildCanvas();
	updateLayerPanelLayout();
	}
}

//--------------------------------------------------------------
void ofApp::clearCanvas(){
	activeLayer().strokes.clear();
	selectedLayerItemType = LayerItemType::Layer;
	drawingStroke = false;
	pendingStroke = false;
	tabletPressureHeld = false;
	activeStroke.points.clear();
	activeStroke.pressures.clear();
	rebuildLayer(activeLayer());
	rebuildCanvas();
	updateLayerPanelLayout();
}

void ofApp::addImageToLayer(Layer &layer, const std::string &imagePath) {
	if (!layer.layerImage.load(imagePath)) {
		return;
	}
	layer.hasImage = true;
	const float imgWidth = layer.layerImage.getWidth();
	const float imgHeight = layer.layerImage.getHeight();
	if (imgWidth > 0 && imgHeight > 0 && canvasWidth > 0 && canvasHeight > 0) {
		layer.imageScale = std::min(canvasWidth / imgWidth, canvasHeight / imgHeight) * 0.5f;
	} else {
		layer.imageScale = 1.0f;
	}
	layer.imagePosition = ofVec2f((canvasWidth - imgWidth * layer.imageScale) * 0.5f,
				(canvasHeight - imgHeight * layer.imageScale) * 0.5f);
	selectedLayerItemType = LayerItemType::Image;
	rebuildLayer(layer);
	rebuildCanvas();
	updateLayerPanelLayout();
}

void ofApp::addImageToActiveLayer(const std::string &imagePath) {
	addImageToLayer(activeLayer(), imagePath);
}

void ofApp::removeImageFromActiveLayer() {
	activeLayer().hasImage = false;
	activeLayer().layerImage.clear();
	if (selectedLayerItemType == LayerItemType::Image) {
		selectedLayerItemType = LayerItemType::Layer;
	}
	rebuildCanvas();
	updateLayerPanelLayout();
}

void ofApp::resetImageTransformForActiveLayer() {
	Layer &layer = activeLayer();
	if (!layer.hasImage) {
		return;
	}
	const float imgWidth = layer.layerImage.getWidth();
	const float imgHeight = layer.layerImage.getHeight();
	if (imgWidth > 0 && imgHeight > 0 && canvasWidth > 0 && canvasHeight > 0) {
		layer.imageScale = std::min(canvasWidth / imgWidth, canvasHeight / imgHeight) * 0.5f;
	} else {
		layer.imageScale = 1.0f;
	}
	layer.imagePosition = ofVec2f((canvasWidth - imgWidth * layer.imageScale) * 0.5f,
				(canvasHeight - imgHeight * layer.imageScale) * 0.5f);
	rebuildLayer(layer);
	rebuildCanvas();
}

