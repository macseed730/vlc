#! /usr/bin/env python3
# -*- coding: utf-8 -*-
import sys
import json
import fontforge
import six
from jinja2 import Template

DOC= """
transform a batch of SVG to a font usable in QML. a QML object is generated to serve as an index

known issues:

  - SVG should not have overlapping
  - SVG should have a viewbox equal to the size of the SVG, ie:
         <svg width="48" height="48" viewBox="0 0 48 48">

Input files must respect the format described by CONFIG_SCHEMA
"""

CONFIG_SCHEMA = {
    "type": "object",
    "properties": {
        "qml_name": { "type" : "string", "description" : "name of the generated QML class {{qml_name}}.qml" },
	"qml_file_prefix": { "type" : "string", "description" : "import path within QML" },
	"font_file": { "type" : "string", "description" : "output font file" },
	"font_name": { "type" : "string", "description": "font family name" },
	"glyphs" : {
            "type" : "array",
            "items" : {
                "type": "object",
                "properties" : {
                    "key" : { "type" : "string", "description": "keyword of the glyph, should be unique within the list" },
                    "path" : { "type" : "string", "description" : "path of the SVG to use for glyph" },
                    "charcode" : { "type" : "string", "description": "utf8 code" },
                },
                "required": ["key", "path"]
            }
        }
    },
    "required": ["glyphs", "qml_name", "font_name", "font_file", "qml_file_prefix"]
}


QML_TEMPLATE=u"""
/**
 * This file was generated by makeIconFont.py, please do not edit by hand
 */
pragma Singleton
import QtQuick 2.11

QtObject {
    readonly property FontLoader fontLoader : FontLoader {
        source: "{{qml_file_prefix}}{{font_file}}"
    }

    readonly property string fontFamily: "{{font_name}}"

    // Icons
{% for f in glyphs %}    readonly property string {{f.key}} : "{{f.charcode}}"
{% endfor %}
}

"""

UTF8_AREA = 0xE000

def genQml(data):
    template = Template(QML_TEMPLATE)
    qmlout = template.render(**data)
    with open(data["qml_name"], "w+") as fd:
        fd.write(qmlout)

def validateModel(model):
    try:
        import jsonschema
    except ImportError:
        return True

    jsonschema.validate(model, CONFIG_SCHEMA)
    return True

def main(model_fd):
    data = json.load(model_fd)
    if not data:
        return

    validateModel(data)

    font = fontforge.font()
    font.familyname = data["font_name"]
    font.fontname = data["font_name"]
    font.design_size = 1024.0

    font.hasvmetrics = True

    font.upos=0
    font.ascent = 1024
    font.descent = 0

    font.hhea_ascent = 1024
    font.hhea_ascent_add = False
    font.hhea_descent = 0
    font.hhea_descent_add = False
    font.hhea_linegap = 0

    font.os2_use_typo_metrics = True
    font.os2_typoascent = 1024
    font.os2_typoascent_add = False
    font.os2_typodescent = 0
    font.os2_typodescent_add = False
    font.os2_typolinegap = 0

    font.os2_winascent = 1024
    font.os2_winascent_add = False
    font.os2_windescent = 0
    font.os2_windescent_add = False

    for i, glyph in enumerate(data["glyphs"]):
        charcode = UTF8_AREA + i
        c = font.createChar(charcode)
        glyph["charcode"]  = "\\u{:x}".format(charcode)
        c.importOutlines(glyph["path"])
        #scale glyph to fit between 200 (base line) and 800 (x 0.6)
        #c.transform((0.6, 0.0, 0.0, 0.6, 200, 200.))
        c.vwidth = 1024
        c.width = 1024

    font.generate(data["font_file"])
    genQml(data)

if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser(description="generate an icon font for QML from SVG")
    parser.add_argument("model", metavar="model",type=argparse.FileType("r"), default=sys.stdin,
                        help="the input model")
    args = parser.parse_args()
    main(args.model)
