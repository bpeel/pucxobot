/*
 * Pucxobot - A robot to play Coup in Esperanto (Puĉo)
 * Copyright (C) 2020  Neil Roberts
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

function WordpartyVisualisation(svg, playerNum, sendMessageCb)
{
  this.svg = svg;

  this.svg.setAttribute("viewBox",
                        "-50 -50 100 " +
                        (WordpartyVisualisation.USED_LETTER_SIZE * 2.0 + 100));
  this.sendMessageCb = sendMessageCb;
  this.players = [];

  this.playerNum = playerNum;
  this.currentPlayer = 0;

  this.arrow = this.createElement("path");
  this.arrow.setAttribute("d",
                          "M -0.01,-23.82 -10.15,-13.70 " +
                          "h 4.63 " +
                          "v 2.94 " +
                          "A 12.10,12.10 0 0 0 -12.10,0 12.10,12.10 0 0 0 " +
                          "0,12.10 12.10,12.10 0 0 0 12.10,0 12.10,12.10 " +
                          "0 0 0 5.49,-10.78 " +
                          "a 12.10,12.10 0 0 0 -0.00,-0.00 " +
                          "v -2.92 " +
                          "h 4.61 " +
                          "z");
  this.arrow.setAttribute("fill", "blue");
  this.arrow.setAttribute("stroke", "black");
  this.arrow.setAttribute("stroke-width", 0.5);
  this.svg.appendChild(this.arrow);

  this.arrowRotation = 0;
  this.arrowAnimation = this.createElement("animateTransform");
  this.arrowAnimation.setAttribute("begin", "indefinite");
  this.arrowAnimation.setAttribute("dur", "0.2s");
  this.arrowAnimation.setAttribute("type", "rotate");
  this.arrowAnimation.setAttribute("attributeName", "transform");
  this.arrow.appendChild(this.arrowAnimation);

  this.syllable = this.createElement("text");
  this.syllable.setAttribute("font-size", WordpartyVisualisation.FONT_SIZE);
  this.syllable.setAttribute("font-family", "sans-serif");
  this.syllable.setAttribute("font-weight", "bold");
  this.syllable.setAttribute("text-anchor", "middle");
  this.syllable.setAttribute("fill", "white");
  this.syllable.setAttribute("x", 0);
  this.syllable.setAttribute("y", WordpartyVisualisation.FONT_SIZE / 2);
  this.svg.appendChild(this.syllable);

  this.resultIcon = null;
  this.resultIconTimer = null;

  this.wrongSound = this.makeSound("wrong.mp3");
  this.correctSound = this.makeSound("correct.mp3");
  this.loseLifeSound = this.makeSound("loselife.mp3");
  this.gainLifeSound = this.makeSound("gainlife.mp3");

  this.makeAlphabet();
}

WordpartyVisualisation.FONT_SIZE = 4;
WordpartyVisualisation.RESULT_SIZE = 12;
WordpartyVisualisation.CIRCLE_RADIUS = 40;
WordpartyVisualisation.USED_LETTER_SIZE = 4;
WordpartyVisualisation.USED_LETTERS_PER_ROW = 14;

WordpartyVisualisation.prototype.createElement = function(tag)
{
  return document.createElementNS("http://www.w3.org/2000/svg", tag);
};

WordpartyVisualisation.prototype.makeSound = function(filename)
{
  return new Audio("../" + filename);
};

WordpartyVisualisation.prototype.getPlayer = function(playerNum)
{
  if (i < this.players.length)
    return this.players[i];

  for (var i = this.players.length; i <= playerNum; i++) {
    var group = this.createElement("g");
    this.svg.appendChild(group);

    var lineHeight = WordpartyVisualisation.FONT_SIZE * 1.4;
    var y = WordpartyVisualisation.FONT_SIZE - lineHeight * 3 / 2;

    var nameElement = this.createElement("text");
    nameElement.setAttribute("font-size", WordpartyVisualisation.FONT_SIZE);
    nameElement.setAttribute("font-family", "sans-serif");
    nameElement.setAttribute("font-weight", "bold");
    nameElement.setAttribute("text-anchor", "middle");
    nameElement.setAttribute("fill", "white");
    nameElement.setAttribute("y", y);
    group.appendChild(nameElement);

    y += lineHeight;

    var livesElement = this.createElement("text");
    livesElement.setAttribute("font-size", WordpartyVisualisation.FONT_SIZE);
    livesElement.setAttribute("font-family", "sans-serif");
    livesElement.setAttribute("text-anchor", "middle");
    livesElement.setAttribute("fill", "white");
    livesElement.setAttribute("y", y);
    group.appendChild(livesElement);

    y += lineHeight;

    var typedWordElement = this.createElement("text");
    typedWordElement.setAttribute("font-size",
                                  WordpartyVisualisation.FONT_SIZE);
    typedWordElement.setAttribute("font-family", "sans-serif");
    typedWordElement.setAttribute("text-anchor", "middle");
    typedWordElement.setAttribute("fill", "white");
    typedWordElement.setAttribute("y", y);
    group.appendChild(typedWordElement);

    var player = {
      "name": nameElement,
      "group": group,
      "lives": livesElement,
      "typedWord": typedWordElement,
      "nLives": -10,
    };

    this.players.push(player);
  }

  this.repositionPlayers();
  this.updateArrow();

  return this.players[playerNum];
};

WordpartyVisualisation.prototype.repositionPlayers = function()
{
  var numPlayers = this.players.length;

  for (var i = 0; i < numPlayers; i++) {
    var angle = (i / numPlayers + 0.75) * 2.0 * Math.PI;
    var x = WordpartyVisualisation.CIRCLE_RADIUS * Math.cos(angle);
    var y = WordpartyVisualisation.CIRCLE_RADIUS * Math.sin(angle);
    this.players[i].group.setAttribute("transform",
                                       "translate(" + x + "," + y + ")");
  }
};

WordpartyVisualisation.prototype.removeResultIcon = function()
{
  if (this.resultIconTimer != null) {
    clearTimeout(this.resultIconTimer);
    this.resultIconTimer = null;
  }

  if (this.resultIcon != null) {
    this.resultIcon.parentElement.removeChild(this.resultIcon);
    this.resultIcon = null;
  }
};

WordpartyVisualisation.prototype.setResultIcon = function(player, icon)
{
  this.removeResultIcon();

  this.resultIcon = this.createElement("text");
  this.resultIcon.setAttribute("font-size", WordpartyVisualisation.RESULT_SIZE);
  this.resultIcon.setAttribute("font-family", "sans-serif");
  this.resultIcon.setAttribute("text-anchor", "middle");
  this.resultIcon.setAttribute("y", WordpartyVisualisation.RESULT_SIZE / 2.0);
  this.resultIcon.setAttribute("opacity", 0);
  this.resultIcon.appendChild(document.createTextNode(icon));

  var anim = this.createElement("animate");
  anim.setAttribute("attributeName", "opacity");
  anim.setAttribute("begin", "indefinite");
  anim.setAttribute("dur", "1s");
  anim.setAttribute("values", "1;1;0");
  this.resultIcon.appendChild(anim);

  player.group.appendChild(this.resultIcon);

  anim.beginElement();

  function timeoutCb()
  {
    this.resultIconTimer = null;
    this.removeResultIcon();
  }

  this.resultIconTimer = setTimeout(timeoutCb.bind(this), 1000);
};

WordpartyVisualisation.prototype.handleResult = function(playerNum, result)
{
  var text;

  if (result == 0) {
    this.correctSound.play();
    return;
  } else if (result == 1) {
    text = "👎️";
  } else if (result == 2) {
    text = "♻️";
  } else {
    return;
  }

  this.wrongSound.play();

  this.setResultIcon(this.getPlayer(playerNum), text);
};

WordpartyVisualisation.prototype.handlePlayerName = function(playerNum, name)
{
  var player = this.getPlayer(playerNum);
  var nameElement = player.name;

  while (nameElement.lastChild)
    nameElement.removeChild(nameElement.lastChild);

  nameElement.appendChild(document.createTextNode(name));
};

WordpartyVisualisation.prototype.handleSidebandData = function(dataNum, mr)
{
  if (dataNum == 0) {
    this.currentPlayer = mr.getUint8();
    this.updateArrow();

    if (this.currentPlayer == this.playerNum) {
      var input = document.getElementById("inputbox");
      if (input)
        input.focus();
    }

    return;
  }

  dataNum--;

  if (dataNum < this.players.length) {
    var player = this.getPlayer(dataNum);
    var val = mr.getUint8();
    var lives;

    if (val == 0) {
      lives = "☠️";
    } else {
      lives = "";

      for (var i = 0; i < val; i++)
        lives += "❤️";
    }

    if (player.nLives == val + 1) {
      this.loseLifeSound.play();
      player.typedWord.setAttribute("text-decoration", "line-through");
    } else if (player.nLives == val - 1) {
      this.gainLifeSound.play();
      this.setResultIcon(player, "💗️");
    }

    player.nLives = val;

    var livesElement = player.lives;
    while (livesElement.lastChild)
      livesElement.removeChild(livesElement.lastChild);
    livesElement.appendChild(document.createTextNode(lives));

    return;
  }

  dataNum -= this.players.length;

  if (dataNum == 0) {
    while (this.syllable.lastChild)
      this.syllable.removeChild(this.syllable.lastChild);
    this.syllable.appendChild(document.createTextNode(mr.getString()));
    return;
  }

  dataNum--;

  if (dataNum == 0) {
    var val = mr.getUint8();
    this.handleResult(val & 0x0f, val >> 6);
    return;
  }

  dataNum--;

  if (dataNum < this.players.length) {
    var player = this.getPlayer(dataNum);

    var typedWordElement = player.typedWord;

    while (typedWordElement.lastChild)
      typedWordElement.removeChild(typedWordElement.lastChild);
    typedWordElement.appendChild(document.createTextNode(mr.getString()));
    typedWordElement.removeAttribute("text-decoration", "line-through");
    return;
  }

  dataNum -= this.players.length;

  if (dataNum < this.players.length) {
    if (dataNum == this.playerNum)
      this.updateUsedLetters(mr.getUint32());
    return;
  }
};

WordpartyVisualisation.prototype.updateArrow = function()
{
  var angle = (this.players.length <= 0 ?
               0.0 :
               this.currentPlayer * 360.0 / this.players.length);

  if (angle == this.arrowRotation)
    return;

  this.arrow.setAttribute("transform", "rotate(" + angle + ")");

  var end = angle < this.arrowRotation ? angle + 360.0 : angle;
  this.arrowAnimation.setAttribute("from", this.arrowRotation);
  this.arrowAnimation.setAttribute("to", end);
  this.arrowAnimation.beginElement();

  this.arrowRotation = angle;
};

WordpartyVisualisation.prototype.handleInputChanged = function(input)
{
  var text = input.trim();

  if (text.match(/\P{Alpha}/u))
    return;

  this.sendMessageCb(0x88, 'Cs', 0x00, input);
};

WordpartyVisualisation.prototype.setUsedLetterState = function(letterNum, state)
{
  var letter = this.lettersUsed[letterNum];

  letter.rect.setAttribute("fill", state ? "none" : "blue");
  letter.text.setAttribute("fill", state ? "grey" : "white");
};

WordpartyVisualisation.prototype.makeAlphabet = function()
{
  var alphabet = "@ALPHABET@";

  var xStart = (WordpartyVisualisation.USED_LETTER_SIZE *
                -WordpartyVisualisation.USED_LETTERS_PER_ROW / 2.0);

  this.lettersUsedGroup = this.createElement("g");
  this.lettersUsedGroup.setAttribute("transform",
                                     "translate(" + xStart + " 50)");

  this.lettersUsed = [];

  var x = 0, y = 0;

  for (var i = 0; i < alphabet.length; i++) {
    var letter = alphabet.substring(i, i + 1);

    var rect = this.createElement("rect");
    rect.setAttribute("x", x);
    rect.setAttribute("y", y);
    rect.setAttribute("width", WordpartyVisualisation.USED_LETTER_SIZE);
    rect.setAttribute("height", WordpartyVisualisation.USED_LETTER_SIZE);

    this.lettersUsedGroup.appendChild(rect);

    var text = this.createElement("text");
    text.appendChild(document.createTextNode(letter));
    text.setAttribute("font-size",
                      WordpartyVisualisation.USED_LETTER_SIZE * 0.5);
    text.setAttribute("font-family", "sans-serif");
    text.setAttribute("font-weight", "bold");
    text.setAttribute("text-anchor", "middle");
    text.setAttribute("x", x + WordpartyVisualisation.USED_LETTER_SIZE * 0.5);
    text.setAttribute("y", y + WordpartyVisualisation.USED_LETTER_SIZE * 0.75);

    this.lettersUsedGroup.appendChild(text);

    this.lettersUsed.push({
      "letter": letter,
      "rect": rect,
      "text": text,
    });

    this.setUsedLetterState(i, false);

    if ((i + 1) % WordpartyVisualisation.USED_LETTERS_PER_ROW == 0) {
      x = 0;
      y += WordpartyVisualisation.USED_LETTER_SIZE;
    } else {
      x += WordpartyVisualisation.USED_LETTER_SIZE;
    }
  }

  /* Sort the letters by unicode character so that we can easily find
   * them by the bit number in the sideband data.
   */
  this.lettersUsed.sort(function(a, b) {
    return a.letter < b.letter ? -1 : 1;
  });

  this.svg.appendChild(this.lettersUsedGroup);
};

WordpartyVisualisation.prototype.updateUsedLetters = function(bitmask)
{
  /* Hide the used letters group if the player can’t gain another
   * life
   */
  if (bitmask == 0xffffffff) {
    if (this.lettersUsedGroup.parentNode)
      this.lettersUsedGroup.parentNode.removeChild(this.lettersUsedGroup);

    return;
  }

  for (var i = 0; i < this.lettersUsed.length; i++)
    this.setUsedLetterState(i, !!(bitmask & (1 << i)));

  if (this.lettersUsedGroup.parentNode == null)
    this.svg.appendChild(this.lettersUsedGroup);
};
