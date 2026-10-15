#define BOOST_TEST_MODULE SVG Tests
#include <boost/test/included/unit_test.hpp>

#include "svg/svgpathlexer.h"
#include "svg/svgfilesplitter.h"
#include "svg/svgflattener.h"
#include "svg/svg2gerber.h"

#include <QTextStream>
#include <QFile>

/*
Testing that svg2gerber path2gerbCommandSlot is not influenced by newlines and whitespace.
*/

#include <algorithm>

#include <boost/lexical_cast.hpp>

BOOST_AUTO_TEST_CASE( svg2gerber_parse )
{
	QString data1 = "M1495.5,1742.5L1504.5,1742.5 M195.5,1743.5L204.5,1743.5 M1495.5,1743.5L1504.5,1743.5 M195.5,1744.5L204.5,1744.5 M1495.5,1744.5L1504.5,1744.5 M195.5,1745.5L1504.5,1745.5 M195.5,1746.5L1504.5,1746.5 M195.5,1747.5L1504.5,1747.5 M195.5,1748.5L1504.5,1748.5 M195.5,1749.5L1504.5,1749.5 \nM195.5,1750.5L1504.5,1750.5 M195.5,1751.5L1504.5,1751.5";

	QString data2 = "M1495.5,1742.5L1504.5,1742.5 M195.5,1743.5L204.5,1743.5 M1495.5,1743.5L1504.5,1743.5 M195.5,1744.5L204.5,1744.5 M1495.5,1744.5L1504.5,1744.5 M195.5,1745.5L1504.5,1745.5 M195.5,1746.5L1504.5,1746.5 M195.5,1747.5L1504.5,1747.5 M195.5,1748.5L1504.5,1748.5 M195.5,1749.5L1504.5,1749.5  M195.5,1750.5L1504.5,1750.5 M195.5,1751.5L1504.5,1751.5";

	SVG2gerber svg2gerber;
	const char * slot = SLOT(path2gerbCommandSlot(QChar, bool, QList<double> &, void *));
	PathUserData pathUserData1;
	pathUserData1.x = 0;
	pathUserData1.y = 0;
	pathUserData1.pathStarting = true;
	pathUserData1.string = "";

	PathUserData pathUserData2;
	pathUserData2.x = 0;
	pathUserData2.y = 0;
	pathUserData2.pathStarting = true;
	pathUserData2.string = "";

	SvgFlattener flattener;
	try {
		flattener.parsePath(data1, slot, pathUserData1, &svg2gerber, true);
		flattener.parsePath(data2, slot, pathUserData2, &svg2gerber, true);
	}
	catch (const QString & msg) {
	}
	catch (char const *str) {
	}
	catch (...) {
	}
	BOOST_CHECK_EQUAL(pathUserData1.string.toStdString(), pathUserData2.string.toStdString());
}

/*
Testing that normalizeChild correctly scales <image> element coordinates
when the SVG viewBox is rescaled during gerber export.
*/
BOOST_AUTO_TEST_CASE( normalize_image_element )
{
	QString svg =
		"<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
		"<svg xmlns=\"http://www.w3.org/2000/svg\" "
		"xmlns:xlink=\"http://www.w3.org/1999/xlink\" "
		"width=\"1in\" height=\"1in\" viewBox=\"0 0 100 100\">"
		"<image x=\"10\" y=\"20\" width=\"80\" height=\"60\" "
		"xlink:href=\"data:image/png;base64,"
		"iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAA"
		"DUlEQVR42mNk+M9QDwADhgGAWjR9awAAAABJRU5ErkJggg==\"/>"
		"</svg>";

	SvgFileSplitter splitter;
	bool loadResult = splitter.splitString(svg, "");
	BOOST_CHECK(loadResult);

	double factor;
	bool result = splitter.normalize(1000.0, "", false, factor);
	BOOST_CHECK(result);

	// Original viewBox: 0 0 100 100, size: 1in x 1in
	// New viewBox: 0 0 1000 1000 => scale factor 10x
	// Expected: x=100, y=200, width=800, height=600
	QDomDocument doc;
	doc.setContent(splitter.toString());
	QDomNodeList images = doc.elementsByTagName("image");
	BOOST_REQUIRE_EQUAL(images.count(), 1);
	QDomElement img = images.at(0).toElement();

	BOOST_CHECK_CLOSE(img.attribute("x").toDouble(), 100.0, 0.01);
	BOOST_CHECK_CLOSE(img.attribute("y").toDouble(), 200.0, 0.01);
	BOOST_CHECK_CLOSE(img.attribute("width").toDouble(), 800.0, 0.01);
	BOOST_CHECK_CLOSE(img.attribute("height").toDouble(), 600.0, 0.01);
}
