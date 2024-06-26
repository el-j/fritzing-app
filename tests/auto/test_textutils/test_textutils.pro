# /*******************************************************************
# Part of the Fritzing project - http://fritzing.org
# Copyright (c) 2019 Fritzing
# Fritzing is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
# Fritzing is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
# You should have received a copy of the GNU General Public License
# along with Fritzing. If not, see <http://www.gnu.org/licenses/>.
# ********************************************************************/

CONFIG += c++17

# specify absolute path so that unit test compiles will find the folder
absolute_boost = 1
include($$absolute_path(../../../pri/boostdetect.pri))
include($$absolute_path(../../../pri/svgppdetect.pri))

QT += core xml
equals(QT_MAJOR_VERSION, 6) {
  QT += core5compat svgwidgets
}
#concurrent core gui network printsupport serialport sql svg widgets xml

HEADERS += $$files(*.h)
SOURCES += $$files(*.cpp)

INCLUDEPATH += $$absolute_path(../../../src)
#INCLUDEPATH += $$top_srcdir

HEADERS += $$files(../../../src/utils/textutils.h)
SOURCES += $$files(../../../src/utils/textutils.cpp)
INCLUDEPATH += $$absolute_path(../../../src/utils)
# FLIBS += textutils

