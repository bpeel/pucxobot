/*
 * Pucxobot - A bot and website to play some card games
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

MessageReader.prototype.getUint32 = function()
{
  var value = this.dv.getUint32(this.pos, true /* littleEndian */);
  this.pos++;
  return value;
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
