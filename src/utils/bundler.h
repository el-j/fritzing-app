/*******************************************************************

Part of the Fritzing project - http://fritzing.org
Copyright (c) 2007-2019 Fritzing

Fritzing is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Fritzing is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Fritzing.  If not, see <http://www.gnu.org/licenses/>.

********************************************************************/

#ifndef BUNDLER_H_
#define BUNDLER_H_

class Bundler {
public:
	virtual ~Bundler() {}
	virtual bool saveAsAux(const QString &filename) = 0;
	virtual bool loadBundledAux(QDir &dir, QList<class ModelPart*> mps) {
		Q_UNUSED(dir);
		Q_UNUSED(mps);
		return true;
	};
	virtual bool preloadBundledAux(QDir &dir, bool dontAsk) {
		Q_UNUSED(dir);
		Q_UNUSED(dontAsk);
		return true;
	};
};

#endif /* BUNDLER_H_ */
