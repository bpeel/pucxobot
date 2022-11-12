/*
 * Puxcobot - A robot to play Coup in Esperanto (Puĉo)
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

  this.words = [];
}

ChameleonVisualisation.TITLE_FONT_SIZE = 8;
ChameleonVisualisation.WORD_FONT_SIZE = 2;
ChameleonVisualisation.LINE_SPLIT_LENGTH = 20;

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
  var middle = value.length / 2;
  var bestPoint = null;
  var bestDistance = value.length + 1;

  for (var i = 0; i < value.length; i++) {
    if (value.charAt(i) == " ") {
      var distance = Math.abs(i - middle);

      if (distance < bestDistance) {
        bestDistance = distance;
        bestPoint = i;
      }
    }
  }

  if (bestPoint === null)
    return false;

  while (elem.lastChild)
    elem.removeChild(elem.lastChild);

  var line1 = this.createElement("tspan");
  line1.appendChild(document.createTextNode(value.substring(0, bestPoint)));
  line1.setAttribute("x", elem.getAttribute("x"));
  line1.setAttribute("dy", -ChameleonVisualisation.WORD_FONT_SIZE / 2);
  elem.appendChild(line1);
  var line2 = this.createElement("tspan");
  line2.appendChild(document.createTextNode(value.substring(bestPoint + 1)));
  line2.setAttribute("x", elem.getAttribute("x"));
  line2.setAttribute("dy", ChameleonVisualisation.WORD_FONT_SIZE * 1.5);
  elem.appendChild(line2);

  return true;
};

ChameleonVisualisation.prototype.handleSidebandData = function(dataNum, mr)
{
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

      if (value.length < ChameleonVisualisation.LINE_SPLIT_LENGTH ||
          !this.splitLine(this.words[wordNum], value))
        this.setTextValue(this.words[wordNum], value);
    }
  }
};
