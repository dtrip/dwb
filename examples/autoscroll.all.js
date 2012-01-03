//
// Copyright (c) 2010-2012 Stefan Bolte <portix@gmx.net>
// Version: GPL 3.0, MIT/X Consortium License
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
//
// Alternatively, the contents of this file may be used under the terms of the
// MIT/X Consortium License in which case the provisions of the MIT License are
// applicable instead of those above. If you wish to allow use of your version
// of this file only under the terms of the MIT License, and not to allow others
// to use your version of this file under the terms of the GPL, indicate your
// decision by deleting the provisions above and replace them with the notice
// and other provisions required by the MIT License. If you do not delete the
// provisions above, a recipient may use your version of this file under the
// terms of any one of the GPL or MIT/X Consortium License.
//

MouseAutoScroll = (function() {
  const SCROLL_ICON="transparent url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAMAAABEpIrGAAAABGdBTUEAALGPC/xhBQAAAAFzUkdCAK7OHOkAAAAgY0hSTQAAeiYAAICEAAD6AAAAgOgAAHUwAADqYAAAOpgAABdwnLpRPAAAAi5QTFRFAAAAAAEAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAQAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAQEAAAAAAAEAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAQEAAAEAAAAAAAAAAAAAAAAAAAAAAAAA////PMNlIQAAAKR0Uk5TAAAAD2p/JQqN+74iBn34sRkDbvT+ohIBX+78kwxQ5/mEB0Pf9XMEM9XwYgCLxhBc1tvc9f7Uu756BgQZHSXC/WIFCQMJu/1cCbv9XAm7/VwJu/1eB7f+YQa1/mEGtf5hBrX+YQgpLzTC/m4RFAhm5+vr+f/i0NOQCILHECPD6lUw0PBmAj3a9nUESuP6hQgAWOv8lA0CZ/H/pBIFdvWxGghQZRvM0CN6AAAAAWJLR0S5OrgWYAAAAAlwSFlzAAALEwAACxMBAJqcGAAAAUJJREFUOMuV0FNzREEQhuET27ZtOxvbtm3btm10bOfnBV0bnJ3Zqv1u3+diphlGsImIionz6xKSUiAtQ++ycvIAJwqKtK6krAJweqaqpk7MQhqaWgBwfnGpraNLAsJ6+p8drq5vbg0MjQjA2AS+d3f/8GhqZs7TLSyBu6fnFytrGzaw/enw+vYOdvZs4ODo5OyCwNXN3cPTiw28fXz9/BFwAgKDgkn/CAlFEBZOOVREJIKoaAqIiUUQF08BCYkIkpIpICUVQVo6BWRkIsjKpoCcXAR5+RRQUIigqJhUS0rLyisQVFZV19TygLr6hsYmBM0trW3tHWzQCf/W1c0GPb1/e18/7yMGBn/70PAI4ZmjY9w+PjFJ/ObUNPaZ2TnyHeYXFr/60vIK5VDM6to6wMbmFkPd9s4u7O0zfHZweHTMCLYPkhas1aCqNgQAAAAldEVYdGRhdGU6Y3JlYXRlADIwMTEtMDQtMDdUMTE6Mjk6MzcrMDI6MDB8AZWGAAAAJXRFWHRkYXRlOm1vZGlmeQAyMDExLTA0LTA3VDExOjI5OjM3KzAyOjAwDVwtOgAAAABJRU5ErkJggg==) no-repeat scroll center";
  const SIZE = 32;
  const OFFSET = 5;
  var doc;
  Math.sign = function(x) { return x >=0 ? 1  : -1; };
  var span = null;
  var x = 0;
  var y = 0;
  var ev = null;
  var timerId = 0;
  var cursorStyle = null;
  function startTimer() {
    timerId = window.setInterval(timer, 40);
  }
  function stopTimer () {
    window.clearInterval(timerId);
    timerId = 0;
  }
  function timer () {
    if (ev) {
      var scrollY = (ev.y - y);
      var scrollX = (ev.x - x);
      var b = scrollY > 0 
        && doc.documentElement.scrollHeight == (window.innerHeight + window.pageYOffset);
      var r = scrollX > 0 
        && doc.documentElement.scrollWidth == (window.innerWidth + window.pageXOffset);
      var l = scrollX < 0 && window.pageXOffset == 0;
      var t = scrollY < 0 && window.pageYOffset == 0;
      var offX = Math.abs(scrollX) < OFFSET;
      var offY = Math.abs(scrollY) < OFFSET;
      if ( timerId != 0 
          && (( b && r ) || ( b && l) || ( b && offX ) 
            || ( t && r ) || ( t && l) || ( t && offX ) 
            || (offY && r) || (offY && l) )) {
              stopTimer();
              return;
            }
      window.scrollBy(scrollX - Math.sign(scrollX) * OFFSET, 
          scrollY - Math.sign(scrollY) * OFFSET);
    }
  }
  function mouseMove (e) { 
    if (timerId == 0) {
      startTimer();
    }
    ev = e; 
  }
  function init (e) {
    doc = e.target.ownerDocument;
    if (window.innerHeight >= doc.documentElement.scrollHeight 
        && window.innerWidth >= doc.documentElement.scrollWidth) {
          return;
        }
    span = doc.createElement("div");
    span.style.width = SIZE + "px";
    span.style.height = SIZE + "px";
    span.style.background = SCROLL_ICON;
    span.style.left = e.x - (SIZE / 2) + "px"; 
    span.style.top  = e.y - (SIZE / 2) + "px"; 
    span.style.position = "fixed";
    span.style.fontSize = SIZE + "px";
    span.style.opacity = 0.6;
    cursorStyle = doc.defaultView.getComputedStyle(doc.body, null).cursor;
    doc.body.style.cursor = "move";
    doc.body.appendChild(span);
    doc.addEventListener('mousemove', mouseMove, false);
    span = span;
  }
  function clear (e) {
    doc.body.style.cursor = cursorStyle;
    span.parentNode.removeChild(span);
    doc.removeEventListener('mousemove', mouseMove, false);
    stopTimer();
    if (span)
      span =  null;
    if (ev)
      ev = null;
  }
  function mouseUp (e) { // Simulate click, click event does not work during scrolling
    if (Math.abs(e.x - x) < 5 && Math.abs(e.y - y) < 5) {
      init(e);
      window.removeEventListener('mouseup', mouseUp, false);
    }
  }
  function mouseDown (e) {
    var t = e.target;
    if (e.button == 1) {
      if (span) {
        clear();
      }
      else if (!t.hasAttribute("href") 
          && !t.hasAttribute("onmousedown") 
          && !(t.hasAttribute("onclick"))) {
        x = e.x;
        y = e.y;
        window.addEventListener('mouseup', mouseUp, false);
      }
    }
  }
  window.addEventListener('mousedown', mouseDown, false);
})();
