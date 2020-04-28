/*
 * Puxcobot - A robot to play Coup in Esperanto (Puĉo)
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

function Pucxo()
{
  this.sock = null;
  this.playerId = null;
  this.messagesDiv = document.getElementById("messages");
};

Pucxo.prototype.utf8ToString = function(ba)
{
  var s = "";
  var i;

  for (i = 0; i < ba.length; i++) {
    var c = ba[i];
    s = s + '%';
    if (c < 16)
      s = s + '0';
    s = s + c.toString(16);
  }

  return decodeURIComponent(s);
};

Pucxo.prototype.doConnect = function()
{
  var location = window.location;

  this.sock = new WebSocket("ws://" + location.hostname + ":3648/");
  this.sock.binaryType = 'arraybuffer';
  this.sock.onerror = this.sockErrorCb.bind(this);
  this.sock.onopen = this.sockOpenCb.bind(this);
  this.sock.onmessage = this.messageCb.bind(this);
};

Pucxo.prototype.sockErrorCb = function(e)
{
  console.log("Error: " + p);
};

Pucxo.prototype.argSizes = {
  "U": 8,
};

Pucxo.prototype.sendMessage = function(msgType, argTypes)
{
  var msgSize = 1;
  var i;

  for (i = 0; i < argTypes.length; i++)
    msgSize += this.argSizes[argTypes.charAt(i)];

  var ab = new ArrayBuffer(msgSize);
  var dv = new DataView(ab);

  dv.setUint8(0, msgType);

  var pos = 1;

  for (i = 0; i < argTypes.length; i++) {
    var arg = arguments[i + 2];
    var t = argTypes.charAt(i);

    if (t == 'U')
      dv.setBigUint64(pos, arg, true);

    pos += this.argSizes[t];
  }

  this.sock.send(ab);
};

Pucxo.prototype.sockOpenCb = function(e)
{
  console.log("connected!");
  if (this.playerId != null)
    this.sendMessage(0x81, "U", this.playerId);
  else
    this.sendMessage(0x80, "");
};

Pucxo.prototype.splitStrings = function(ba, pos)
{
  var res = [];

  while (pos < ba.length) {
    var end = pos;

    while (ba[end] != 0)
      end++;

    var s = new Uint8Array(ba.buffer, ba.byteOffset + pos, end - pos);
    res.push(this.utf8ToString(s));

    pos = end + 1;
  }

  return res;
};

Pucxo.prototype.pushButton = function(buttonData)
{
  var buf = new Uint8Array(buttonData.length + 1);
  buf[0] = 0x82;

  var i;

  for (i = 0; i < buttonData.length; i++)
    buf[i + 1] = buttonData.charCodeAt(i);

  this.sock.send(buf);
};

Pucxo.prototype.messageCb = function(e)
{
  var dv = new DataView(e.data);
  var msgType = dv.getUint8(0);

  console.log(msgType);

  if (msgType == 0) {
    this.playerId = dv.getBigUint64(dv, 1, true);
    console.log("playerId " + this.playerId);
  } else if (msgType == 1) {
    var parts = this.splitStrings(new Uint8Array(e.data), 2);

    var lines = parts[0].split("\n");

    var i;

    for (i = 0; i < lines.length; i++) {
      var elem = document.createElement("div");
      elem.appendChild(document.createTextNode(lines[i]));
      this.messagesDiv.appendChild(elem);
    }

    if (parts.length > 1) {
      var d = document.createElement("div");
      for (i = 1; i < parts.length; i += 2) {
        var button = document.createElement("button");
        button.appendChild(document.createTextNode(parts[i]));
        button.onclick = this.pushButton.bind(this, parts[i + 1]);
        d.appendChild(button);
      }
      this.messagesDiv.appendChild(d);
    }
  }
};

Pucxo.prototype.start = function()
{
  this.sendMessage(0x84, "");
};

(function ()
 {
   var puxco;

   function loadCb()
   {
     pucxo = new Pucxo();
     pucxo.doConnect();

     document.getElementById("startButton").onclick = function() {
       pucxo.start();
     }
   }

   window.onload = loadCb;
 }) ();
