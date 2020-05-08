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

function MessageReader(dv, pos)
{
  this.dv = dv;
  this.pos = 0;
}

MessageReader.prototype.getString = function()
{
  var s = "";

  while (!this.isFinished()) {
    var c = this.getUint8();

    if (c == 0)
      break;

    s = s + '%';
    if (c < 16)
      s = s + '0';
    s = s + c.toString(16);
  }

  return decodeURIComponent(s);
};

MessageReader.prototype.getUint64 = function()
{
  var a = new Uint8Array(8);
  var i;

  for (i = 0; i < 8; i++)
    a[i] = this.getUint8();

  return a;
};

MessageReader.prototype.getUint8 = function()
{
  return this.dv.getUint8(this.pos++);
};

MessageReader.prototype.isFinished = function()
{
  return this.pos >= this.dv.byteLength;
};

function Pucxo()
{
  this.sock = null;
  this.sockHandlers = [];
  this.connected = false;
  this.playerId = null;
  this.playerName = null;
  this.isPrivate = null;
  this.messagesDiv = document.getElementById("messages");
  this.reconnectTimeout = null;
  this.numMessagesReceived = 0;
  this.reconnectCount = 0;
  this.unreadMessages = 0;
  this.focused = true;

  this.keepAliveTimeout = null;

  this.statusMessage = document.getElementById("statusMessage");

  this.namebox = document.getElementById("namebox");
  this.namebox.onkeydown = this.nameboxKeyCb.bind(this);

  this.gameType = null;

  this.makeGameButtons();

  this.checkQueryString();

  document.getElementById("chosenName").onclick =
    this.nameChosen.bind(this);
  this.updateChosenNameButton();

  document.getElementById("publicGame").onclick =
    this.setPrivacy.bind(this, false);
  document.getElementById("privateGame").onclick =
    this.setPrivacy.bind(this, true);

  this.namebox.oninput = this.updateChosenNameButton.bind(this);
  this.namebox.onpropertychange = this.updateChosenNameButton.bind(this);

  this.inputbox = document.getElementById("inputbox");
  this.inputbox.onkeydown = this.inputboxKeyCb.bind(this);

  document.getElementById("sendButton").onclick =
    this.sendChatMessage.bind(this);

  window.onunload = this.unloadCb.bind(this);

  window.onhashchange = this.checkHash.bind(this);

  window.onfocus = this.onFocusCb.bind(this);
  window.onblur = this.onBlurCb.bind(this);

  this.checkHash();
};

Pucxo.prototype.checkQueryString = function()
{
  var search = window.location.search;

  if (!search || search.length != 17) {
    this.privateGameId = null;
    return;
  }

  var i;
  this.privateGameId = new Uint8Array(8);

  for (i = 0; i < 8; i++) {
    this.privateGameId[i] =
      parseInt("0x" + search.substring(i * 2 + 1, i * 2 + 3));
  }

  document.getElementById("chosenName").innerText = "@JOIN@";
};

Pucxo.GAMES = [
  { "title": "@COUP_TITLE@", "keyword": "coup" },
  { "title": "@LOVE_LETTER_TITLE@", "keyword": "loveletter", "help": "love" },
  { "title": "@SIX_TITLE@", "keyword": "six" },
  { "title": "@SNITCH_TITLE@", "keyword": "snitch" },
  { "title": "@ZOMBIE_DICE_TITLE@", "keyword": "zombie" },
  { "title": "@SUPERFIGHT_TITLE@", "keyword": "superfight" },
];

Pucxo.prototype.makeGameButtons = function()
{
  var buttonDiv = document.getElementById("chooseGame");

  this.gameButtons = [];

  var enabledGames = "@ENABLED_GAMES@".split(" ");

  var i, j;
  for (j = 0; j < enabledGames.length; j++) {
    for (i = 0; i < Pucxo.GAMES.length; i++) {
      if (Pucxo.GAMES[i].keyword != enabledGames[j])
        continue;

      var button = document.createElement("button");
      button.appendChild(document.createTextNode(Pucxo.GAMES[i].title));
      button.onclick =
        this.gameButtonClickCb.bind(this, Pucxo.GAMES[i]);
      this.gameButtons.push(button);
      buttonDiv.append(button);
      break;
    }
  }
};

Pucxo.prototype.isNameEntered = function()
{
  return this.namebox.value.match(/\S/) != null;
};

Pucxo.prototype.updateChosenNameButton = function()
{
  document.getElementById("chosenName").disabled = !this.isNameEntered();
};

Pucxo.prototype.start = function()
{
  document.getElementById("welcomeOverlay").style.display = "none";

  this.unreadMessages = 0;

  if (!this.connected) {
    this.reconnectCount = 0;
    this.doConnect();
  }
};

Pucxo.prototype.setWelcomeStep = function(step)
{
  var elem;

  this.disconnect();
  this.clearMessages();
  this.setTitle("@TITLE@");
  this.setHelpAnchor("");
  this.clearStatusMessage();

  var overlay = document.getElementById("welcomeOverlay");

  for (elem = overlay.firstElementChild;
       elem != null;
       elem = elem.nextElementSibling) {
    if (elem.id == step)
      elem.style.display = "flex";
    else
      elem.style.display = "none";
  }

  overlay.style.removeProperty("display");
};

Pucxo.prototype.checkHash = function()
{
  var hash = window.location.hash;

  if (!hash || hash == "") {
    this.setWelcomeStep("chooseName");
    return;
  } else if (hash == "#choosePrivacy") {
    if (this.playerName != null &&
        this.privateGameId == null) {
      this.setWelcomeStep("choosePrivacy");
      return;
    }
  } else if (hash == "#chooseGame") {
    if (this.playerName != null &&
        this.isPrivate != null &&
        this.privateGameId == null) {
      this.setWelcomeStep("chooseGame");
      return;
    }
  } else if (hash == "#play") {
    if (this.playerName != null &&
        (this.privateGameId != null ||
         (this.isPrivate != null &&
          (this.gameType != null)))) {
      this.start();
      return
    }
  }

  window.location.hash = '';
};

Pucxo.prototype.nameChosen = function(event)
{
  if (!this.isNameEntered())
    return;

  this.playerName = this.namebox.value;
  this.gameType = null;
  this.isPrivate = null;
  window.location.hash = this.privateGameId ? "#play" : "#choosePrivacy";
}

Pucxo.prototype.setPrivacy = function(privacy)
{
  this.isPrivate = privacy;
  this.gameType = null;
  window.location.hash = "#chooseGame";
};

Pucxo.prototype.nameboxKeyCb = function(event)
{
  if (event.which == 10 || event.which == 13)
    this.nameChosen();
};

Pucxo.prototype.inputboxKeyCb = function(event)
{
  if (event.which == 10 || event.which == 13) {
    event.preventDefault();
    this.sendChatMessage();
  }
};

Pucxo.prototype.sendChatMessage = function()
{
  if (!this.connected)
    return;

  var text = this.inputbox.innerText.replace(/[\x01- ]/g, ' ');

  if (text.match(/\S/) == null)
    return;

  this.inputbox.innerHTML = "";

  this.sendMessage(0x85, 's', text);
};

Pucxo.prototype.gameButtonClickCb = function(gameType)
{
  if (this.isNameEntered()) {
    this.gameType = gameType;
    /* Clear the player ID to make sure it starts a new game when
     * connecting
     */
    this.playerId = null;
    document.location.hash = "#play";
  }
};

Pucxo.prototype.stringToUtf8 = function(s)
{
  s = encodeURIComponent(s);

  var length = 0;
  var i;

  for (i = 0; i < s.length; i++) {
    if (s[i] == '%')
      i += 2;
    length++;
  }

  var ba = new Uint8Array(length);
  var p = 0;

  for (i = 0; i < s.length; i++) {
    if (s[i] == '%') {
      ba[p++] = parseInt("0x" + s.substring(i + 1, i + 3));
      i += 2;
    } else {
      ba[p++] = s.charCodeAt(i);
    }
  }

  return ba;
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

  this.setStatusConnecting();

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

Pucxo.prototype.disconnect = function()
{
  if (this.reconnectTimeout != null) {
    clearTimeout(this.reconnectTimeout);
    this.reconnectTimeout != null;
  }

  if (this.sock == null)
    return;

  this.removeSocketHandlers();
  this.clearKeepAliveTimeout();
  this.sock.close();
  this.sock = null;
  this.connected = false;
};

Pucxo.prototype.clearStatusMessage = function()
{
  this.statusMessage.style.display = "none";
};

Pucxo.prototype.setStatusConnecting = function()
{
  this.statusMessage.innerText = "@CONNECTING@";
  this.statusMessage.style.display = "block";
  this.statusMessage.className = "trying";
};

Pucxo.prototype.setStatusError = function()
{
  this.statusMessage.innerText = "@ERROR@";
  this.statusMessage.style.display = "block";
  this.statusMessage.className = "error";
};

Pucxo.prototype.sockErrorCb = function(e)
{
  console.log("Error on socket: " + e);
  this.disconnect();

  if (++this.reconnectCount >= 10) {
    this.setStatusError();
  } else {
    this.setStatusConnecting();
    if (this.reconnectTimeout == null) {
      this.reconnectTimeout = setTimeout(this.reconnectTimeoutCb.bind(this),
                                         30000);
    }
  }
};

Pucxo.ARG_SIZES = {
  "B": 8,
  "W": 2,
};

Pucxo.prototype.sendMessage = function(msgType, argTypes)
{
  var msgSize = 1;
  var i;
  var stringArgs = null;

  for (i = 0; i < argTypes.length; i++) {
    var ch = argTypes.charAt(i);
    if (ch == "s") {
      if (stringArgs == null)
        stringArgs = [];
      stringArgs.push(this.stringToUtf8(arguments[i + 2]));
      msgSize += stringArgs[stringArgs.length - 1].length + 1;
    }
    else {
      msgSize += Pucxo.ARG_SIZES[ch];
    }
  }

  var ab = new ArrayBuffer(msgSize);
  var dv = new DataView(ab);

  dv.setUint8(0, msgType);

  var pos = 1;
  var stringArg = 0;

  for (i = 0; i < argTypes.length; i++) {
    var arg = arguments[i + 2];
    var t = argTypes.charAt(i);

    if (t == 'B') {
      var j;
      for (j = 0; j < 8; j++)
        dv.setUint8(pos++, arg[j]);
    } else if (t == 'W') {
      dv.setUint16(pos, arg, true);
      pos += 2;
    } else if (t == 's') {
      arg = stringArgs[stringArg++];
      var j;
      for (j = 0; j < arg.length; j++)
        dv.setUint8(pos++, arg[j]);
      dv.setUint8(pos++, 0);
    }
  }

  this.sock.send(ab);

  this.resetKeepAliveTimeout();
};

Pucxo.prototype.sockOpenCb = function(e)
{
  console.log("connected!");

  this.connected = true;

  if (this.playerId != null) {
    this.sendMessage(0x81, "BW", this.playerId, this.numMessagesReceived);
  } else if (this.privateGameId != null) {
    this.sendMessage(0x87, "sB", this.playerName, this.privateGameId);
  } else {
    var command = this.isPrivate ? 0x86 : 0x80;
    this.sendMessage(command, "sss",
                     this.playerName,
                     this.gameType.keyword,
                     "@LANG_CODE@");
  }
};

Pucxo.prototype.pushButton = function(buttonData)
{
  if (!this.connected)
    return;

  var buf = new Uint8Array(buttonData.length + 1);
  buf[0] = 0x82;

  var i;

  for (i = 0; i < buttonData.length; i++)
    buf[i + 1] = buttonData.charCodeAt(i);

  this.sock.send(buf);
};

Pucxo.messageTypeClasses = [
  "public",
  "private",
  "chatOther",
  "chatYou",
];

Pucxo.prototype.handleMessage = function(mr)
{
  this.numMessagesReceived++;

  var messageFlags = mr.getUint8();
  var isHtml = (messageFlags & 1) != 0;
  var messageType = (messageFlags >> 1) & 3;

  var isScrolledToBottom = (this.messagesDiv.scrollHeight -
                            this.messagesDiv.scrollTop <=
                            this.messagesDiv.clientHeight * 1.75);

  var text = mr.getString();

  var lines = text.split("\n");

  var i;

  var messageDiv = document.createElement("div");
  messageDiv.className = "message " + Pucxo.messageTypeClasses[messageType];

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

  if (!mr.isFinished()) {
    var buttonsDiv = document.createElement("div");
    buttonsDiv.className = "messageButtons";

    while (!mr.isFinished()) {
      var button = document.createElement("button");
      button.appendChild(document.createTextNode(mr.getString()));
      button.onclick = this.pushButton.bind(this, mr.getString());
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

  if (!this.focused) {
    this.unreadMessages++;

    if (this.gameType)
      document.title = "(" + this.unreadMessages + ") " + this.gameType.title;
  }
};

Pucxo.prototype.clearMessages = function()
{
  this.numMessagesReceived = 0;
  this.messagesDiv.innerHTML = "";
};

Pucxo.prototype.handlePlayerId = function(mr)
{
  this.playerId = mr.getUint64();

  /* If we get a player ID then we can assume the connection was worked */
  this.clearStatusMessage();
  this.reconnectCount = 0;
};

Pucxo.prototype.addServiceNote = function(note)
{
  var messageDiv = document.createElement("div");
  messageDiv.className = "message " + Pucxo.messageTypeClasses[1];

  var innerDiv = document.createElement("div");
  innerDiv.className = "messageInner";

  var textDiv = document.createElement("div");
  textDiv.className = "messageText";

  textDiv.innerHTML = note;

  innerDiv.appendChild(textDiv);
  messageDiv.appendChild(innerDiv);
  this.messagesDiv.appendChild(messageDiv);
};

Pucxo.prototype.handlePrivateGameId = function(mr)
{
  /* Don’t add the link message if there are already other messages
   * such as after a reconnect.
   */
  if (this.messagesDiv.firstElementChild)
    return;

  var messageDiv = document.createElement("div");
  messageDiv.className = "message " + Pucxo.messageTypeClasses[0];

  var innerDiv = document.createElement("div");
  innerDiv.className = "messageInner";

  var textDiv = document.createElement("div");
  textDiv.className = "messageText";

  var link = (window.location.protocol + "//" +
              window.location.host +
              window.location.pathname + "?");

  var id = mr.getUint64();
  var i;

  for (i = 0; i < 8; i++) {
    var b = id[i];
    if (b < 0x10)
      link += "0";
    link += b.toString(16);
  }

  var note = ("@PRIVATE_LINK@" +
              "<br><br><a target=\"_blank\" href=\"" + link + "\">" +
              link + "</a>");

  this.addServiceNote(note);
};

Pucxo.prototype.setTitle = function(title)
{
  document.title = title;
  document.getElementById("chatTitleText").innerText = title;
};

Pucxo.prototype.setHelpAnchor = function(help)
{
  var helpButton = document.getElementById("helpButton");
  helpButton.href = "help.html#" + help;
};

Pucxo.prototype.handleGameType = function(mr)
{
  var typeName = mr.getString();
  var i;

  for (i = 0; i < Pucxo.GAMES.length; i++) {
    var game = Pucxo.GAMES[i];

    if (game.keyword == typeName) {
      this.gameType = game;

      this.setTitle(this.gameType.title);
      this.setHelpAnchor(this.gameType.help || this.gameType.keyword);

      break;
    }
  }
};

Pucxo.prototype.handlePrivateGameNotFound = function(e)
{
  this.addServiceNote("@PRIVATE_LINK_INVALID@");
  this.disconnect();
};

Pucxo.prototype.messageCb = function(e)
{
  var mr = new MessageReader(new DataView(e.data));
  var msgType = mr.getUint8();

  if (msgType == 0) {
    this.handlePlayerId(mr);
  } else if (msgType == 3) {
    this.handlePrivateGameId(mr);
  } else if (msgType == 1) {
    this.handleMessage(mr);
  } else if (msgType == 2) {
    this.handleGameType(mr);
  } else if (msgType == 4) {
    this.handlePrivateGameNotFound(mr);
  }
};

Pucxo.prototype.unloadCb = function()
{
  /* Try to let the server know the player is going. */
  if (this.connected)
    this.sendMessage(0x84, "");
};

Pucxo.prototype.onFocusCb = function()
{
  this.focused = true;

  if (this.unreadMessages > 0) {
    this.unreadMessages = 0;
    if (this.gameType && this.connected)
      document.title = this.gameType.title;
  }
};

Pucxo.prototype.onBlurCb = function()
{
  this.focused = false;
};

(function ()
 {
   var puxco;

   function loadCb()
   {
     pucxo = new Pucxo();
   }

   window.onload = loadCb;
 }) ();
