/*
 * Pucxobot - A robot to play Coup in Esperanto (Puĉo)
 * Copyright (C) 2022  Neil Roberts
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

function ChameleonVisualisation(svg, playerNum, sendMessageCb)
{
  this.svg = svg;

  this.svg.setAttribute("viewBox", "0 0 100 100");

  this.topic = this.createTextElement(ChameleonVisualisation.TITLE_FONT_SIZE,
                                      50,
                                      10);
  this.topic.setAttribute("font-weight", "bold");
  this.svg.appendChild(this.topic);

  this.cover = this.createTextElement(ChameleonVisualisation.TITLE_FONT_SIZE,
                                      50,
                                      50);
  this.setTextValue(this.cover, "@CHAMELEON_TITLE@ 🦎");
  this.svg.appendChild(this.cover);

  this.words = [];
}

ChameleonVisualisation.TITLE_FONT_SIZE = 8;
ChameleonVisualisation.WORD_FONT_SIZE = 3;
ChameleonVisualisation.LINE_SPLIT_LENGTH = 10;

ChameleonVisualisation.prototype.createTextElement = function(fontSize,
                                                              x,
                                                              y)
{
  var elem = this.createElement("text");
  elem.setAttribute("font-size", fontSize);
  elem.setAttribute("font-family", "sans-serif");
  elem.setAttribute("text-anchor", "middle");
  elem.setAttribute("fill", "white");
  elem.setAttribute("x", x);
  elem.setAttribute("y", y + fontSize / 2.0);
  this.svg.appendChild(elem);
  return elem;
};

ChameleonVisualisation.prototype.createElement = function(tag)
{
  return document.createElementNS("http://www.w3.org/2000/svg", tag);
};

ChameleonVisualisation.prototype.handlePlayerName = function(playerNum, name)
{
};

ChameleonVisualisation.prototype.setTextValue = function(elem, value)
{
  while (elem.lastChild)
    elem.removeChild(elem.lastChild);

  elem.appendChild(document.createTextNode(value));
};

ChameleonVisualisation.prototype.splitLine = function(elem, value)
{
  var lines = [];
  var nSplitPoints = Math.floor(value.length /
                                ChameleonVisualisation.LINE_SPLIT_LENGTH);

  for (var split = 0; split < nSplitPoints; split++) {
    var target = value.length / (nSplitPoints + 1);
    var bestPoint = null;
    var bestDistance = value.length + 1;

    for (var i = 0; i < value.length; i++) {
      if (value.charAt(i) == " ") {
        var distance = Math.abs(i - target);

        if (distance < bestDistance) {
          bestDistance = distance;
          bestPoint = i;
        }
      }
    }

    if (bestPoint === null)
      break;

    lines.push(value.substring(0, bestPoint));
    value = value.substring(bestPoint + 1);
  }

  lines.push(value);

  while (elem.lastChild)
    elem.removeChild(elem.lastChild);

  for (var i = 0; i < lines.length; i++) {
    var line = this.createElement("tspan");
    line.appendChild(document.createTextNode(lines[i]));
    line.setAttribute("x", elem.getAttribute("x"));
    line.setAttribute("dy",
                      i == 0 ?
                      (lines.length - 1) *
                      1.5 *
                      -ChameleonVisualisation.WORD_FONT_SIZE /
                      2.0 :
                      ChameleonVisualisation.WORD_FONT_SIZE * 1.5);
    elem.appendChild(line);
  }
};

ChameleonVisualisation.prototype.handleSidebandData = function(dataNum, mr)
{
  if (this.cover) {
    this.svg.removeChild(this.cover);
    this.cover = null;
  }

  if (dataNum == 0) {
    this.setTextValue(this.topic, mr.getString());
  } else {
    var wordNum = dataNum - 1;
    var value = mr.getString();

    if (value == "") {
      if (this.words[wordNum] !== undefined) {
        this.svg.removeChild(this.words[wordNum]);
        delete this.words[wordNum];
      }
    } else {
      if (this.words[wordNum] === undefined) {
        var elem =
            this.createTextElement(ChameleonVisualisation.WORD_FONT_SIZE,
                                   wordNum % 4 * 25 + 12.5,
                                   Math.floor(wordNum / 4) * 20 + 30);
        this.svg.appendChild(elem);
        this.words[wordNum] = elem;
      }

      this.splitLine(this.words[wordNum], value);
    }
  }
};
