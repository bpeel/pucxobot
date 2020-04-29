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
  this.sockHandlers = [];
  this.connected = false;
  this.playerId = null;
  this.messagesDiv = document.getElementById("messages");
  this.reconnectTimeout = null;
  this.numMessagesDisplayed = 0;
  /* After a reconnect, this will be reset to zero and we’ll ignore
   * messages until we get back to numMessagesDisplayed.
   */
  this.numMessagesReceived = 0;

  this.keepAliveTimeout = null;

  window.onunload = this.unloadCb.bind(this);
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

Pucxo.prototype.clearKeepAliveTimeout = function()
{
  if (this.keepAliveTimeout) {
    clearTimeout(this.keepAliveTimeout);
    this.keepAliveTimeout = null;
  }
};

Pucxo.prototype.resetKeepAliveTimeout = function()
{
  this.clearKeepAliveTimeout();

  function callback()
  {
    this.keepAliveTimout = null;
    if (this.sock)
      this.sendMessage(0x83, "");
  }

  this.keepAliveTimeout = setTimeout(callback.bind(this), 60 * 1000);
};

Pucxo.prototype.addSocketHandler = function(event, func)
{
  this.sockHandlers.push(event, func);
  this.sock.addEventListener(event, func);
}

Pucxo.prototype.removeSocketHandlers = function(event, func)
{
  var i;

  for (i = 0; i < this.sockHandlers.length; i += 2) {
    this.sock.removeEventListener(this.sockHandlers[i],
                                  this.sockHandlers[i + 1]);
  }

  this.sockHandlers = [];
}

Pucxo.prototype.doConnect = function()
{
  var location = window.location;

  console.log("Connecting…");

  this.numMessagesReceived = 0;

  this.sock = new WebSocket("ws://" + location.hostname + ":3648/");
  this.sock.binaryType = 'arraybuffer';
  this.addSocketHandler("error", this.sockErrorCb.bind(this));
  this.addSocketHandler("close", this.sockErrorCb.bind(this));
  this.addSocketHandler("open", this.sockOpenCb.bind(this));
  this.addSocketHandler("message", this.messageCb.bind(this));
};

Pucxo.prototype.reconnectTimeoutCb = function()
{
  this.reconnectTimeout = null;
  this.doConnect();
};

Pucxo.prototype.sockErrorCb = function(e)
{
  console.log("Error on socket: " + e);
  this.removeSocketHandlers();
  this.clearKeepAliveTimeout();
  this.sock.close();
  this.sock = null;
  this.connected = false;

  if (this.reconnectTimeout == null) {
    this.reconnectTimeout = setTimeout(this.reconnectTimeoutCb.bind(this),
                                       30000);
  }
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

  this.resetKeepAliveTimeout();
};

Pucxo.prototype.sockOpenCb = function(e)
{
  console.log("connected!");

  this.connected = true;

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

Pucxo.prototype.handleMessage = function(dv)
{
  /* Skip messages after a reconnect until we get back to where we were. */
  if (this.numMessagesReceived++ < this.numMessagesDisplayed)
    return;
  this.numMessagesDisplayed++;

  var messageFlags = dv.getUint8(1);
  var isHtml = (messageFlags & 1) != 0;

  var isScrolledToBottom = (this.messagesDiv.scrollHeight -
                            this.messagesDiv.scrollTop <=
                            this.messagesDiv.clientHeight + 5);

  var parts = this.splitStrings(new Uint8Array(dv.buffer), 2);

  var lines = parts[0].split("\n");

  var i;

  var messageDiv = document.createElement("div");
  messageDiv.className = "message";

  var innerDiv = document.createElement("div");
  innerDiv.className = "messageInner";

  var textDiv = document.createElement("div");
  textDiv.className = "messageText";

  for (i = 0; i < lines.length; i++) {
    var node;
    if (isHtml) {
      node = document.createElement("span");
      node.innerHTML = lines[i];
    } else {
      node = document.createTextNode(lines[i]);
    }
    textDiv.appendChild(node);
    if (i < lines.length - 1) {
      var br = document.createElement("br");
      textDiv.appendChild(br);
    }
  }

  innerDiv.appendChild(textDiv);

  if (parts.length > 1) {
    var buttonsDiv = document.createElement("div");
    buttonsDiv.className = "messageButtons";

    for (i = 1; i < parts.length; i += 2) {
      var button = document.createElement("button");
      button.appendChild(document.createTextNode(parts[i]));
      button.onclick = this.pushButton.bind(this, parts[i + 1]);
      buttonsDiv.appendChild(button);
    }

    innerDiv.appendChild(buttonsDiv);

  }

  messageDiv.appendChild(innerDiv);

  this.messagesDiv.appendChild(messageDiv);

  if (isScrolledToBottom) {
    this.messagesDiv.scrollTo(0,
                              this.messagesDiv.scrollHeight -
                              this.messagesDiv.clientHeight);
  }
};

Pucxo.prototype.handlePlayerId = function(dv)
{
  this.playerId = dv.getBigUint64(1, true);

  console.log("playerId " + this.playerId);

  /* If the server sends a player ID then it means we’ve started a new
   * player. This can happen even after an attempt to reconnect if the
   * player has timed out and disappeared.
   */
  this.numMessagesDisplayed = 0;
  this.numMessagesReceived = 0;
  this.messagesDiv.innerHTML = "";
};

Pucxo.prototype.messageCb = function(e)
{
  var dv = new DataView(e.data);
  var msgType = dv.getUint8(0);

  console.log(msgType);

  if (msgType == 0) {
    this.handlePlayerId(dv);
  } else if (msgType == 1) {
    this.handleMessage(dv);
  }
};

Pucxo.prototype.start = function()
{
  this.sendMessage(0x84, "");
};

Pucxo.prototype.unloadCb = function()
{
  /* Try to let the server know the player is going. */
  if (this.connected)
    this.sendMessage(0x85, "");
};

(function ()
 {
   var puxco;

   function loadCb()
   {
     pucxo = new Pucxo();
     pucxo.doConnect();
   }

   window.onload = loadCb;
 }) ();
