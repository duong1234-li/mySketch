#include "ofApp.h"

//--------------------------------------------------------------
void ofApp::detectFillPixelOrientation(){
	strokeLayer.begin();
	ofClear(0, 0, 0, 0);
	ofPushStyle();
	ofSetColor(255, 0, 0, 255);
	ofDrawRectangle(8, 0, 12, 12);
	ofPopStyle();
	strokeLayer.end();

	ofPixels pixels;
	strokeLayer.readToPixels(pixels);
	const int pixelHeight = static_cast<int>(pixels.getHeight());
	int markerRow = -1;
	for (int y = 0; y < pixelHeight && markerRow < 0; ++y) {
		for (int x = 8; x < 20; ++x) {
			const ofColor pixel = pixels.getColor(x, y);
			if (pixel.r > 200 && pixel.g < 40) {
				markerRow = y;
				break;
			}
		}
	}
	fillReadbackFlipped = markerRow > pixelHeight / 2;

	strokeLayer.begin();
	ofClear(0, 0, 0, 0);
	strokeLayer.end();
}

//--------------------------------------------------------------
void ofApp::fillAt(int x, int y){
	const int width = canvasWidth;
	const int height = canvasHeight;
	if (x < 0 || x >= width || y < 0 || y >= height) {
		return;
	}

	ofPixels pixels;
	canvas.readToPixels(pixels);
	const int pixelY = fillReadbackFlipped ? height - 1 - y : y;
	const ofColor target = pixels.getColor(x, pixelY);
	ofColor fillColor = brushColor;
	fillColor.a = brushOpacity;
	const int tolerance = fillTolerance;
	auto matches = [tolerance](const ofColor &left, const ofColor &right){
		return std::abs(static_cast<int>(left.r) - right.r) <= tolerance &&
			std::abs(static_cast<int>(left.g) - right.g) <= tolerance &&
			std::abs(static_cast<int>(left.b) - right.b) <= tolerance &&
			std::abs(static_cast<int>(left.a) - right.a) <= tolerance;
	};
	if (matches(target, fillColor)) {
		return;
	}

	const std::size_t pixelCount = static_cast<std::size_t>(width) * height;
	std::vector<unsigned char> visited(pixelCount, 0);
	std::vector<unsigned char> filled(pixelCount, 0);
	std::vector<int> pending;
	pending.reserve(4096);
	auto enqueue = [&](int px, int py){
		if (px < 0 || px >= width || py < 0 || py >= height) {
			return;
		}
		const std::size_t index = static_cast<std::size_t>(py) * width + px;
		if (!visited[index]) {
			visited[index] = 1;
			pending.push_back(static_cast<int>(index));
		}
	};

	enqueue(x, pixelY);
	for (std::size_t cursor = 0; cursor < pending.size(); ++cursor) {
		const int index = pending[cursor];
		const int px = index % width;
		const int py = index / width;
		if (!matches(pixels.getColor(px, py), target)) {
			continue;
		}
		filled[static_cast<std::size_t>(index)] = 1;
		enqueue(px - 1, py);
		enqueue(px + 1, py);
		enqueue(px, py - 1);
		enqueue(px, py + 1);
	}

	Stroke fill;
	fill.color = fillColor;
	fill.width = 1.0f;
	fill.tool = Tool::Fill;
	fill.eraser = false;
	for (int rawY = 0; rawY < height; ++rawY) {
		int px = 0;
		while (px < width) {
			const std::size_t index = static_cast<std::size_t>(rawY) * width + px;
			if (!filled[index]) {
				++px;
				continue;
			}
			const int start = px;
			while (px < width && filled[static_cast<std::size_t>(rawY) * width + px]) {
				++px;
			}
			const int drawY = fillReadbackFlipped ? height - 1 - rawY : rawY;
			fill.fillSpans.push_back({start, drawY, px - start});
		}
	}
	if (fill.fillSpans.empty()) {
		return;
	}

	Layer &layer = activeLayer();
	for (auto &span : fill.fillSpans) {
		const ofVec2f start = canvasPointToLayer(layer, ofVec2f(span.x, span.y));
		const ofVec2f end = canvasPointToLayer(layer, ofVec2f(span.x + span.width, span.y));
		span.x = static_cast<int>(std::lround(start.x));
		span.y = static_cast<int>(std::lround(start.y));
		span.width = std::max(1, static_cast<int>(std::lround(end.x)) - span.x);
	}
	layer.strokes.push_back(std::move(fill));
	applyStroke(layer, layer.strokes.back());
	rebuildCanvas();
}
