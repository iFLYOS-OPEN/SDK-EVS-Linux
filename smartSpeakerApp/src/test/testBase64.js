var Buffer = require('buffer')
var console = require('console')

var str = 'sGB8+S1oWOhQwXF88+8GXc03/ycxD56HL4cyvR18a/e3Vb2AKmgWg4M+AjSDSAYZVbZSNmDrCwvnNzyEBsJeKw==';
var buf = new Buffer(str, 'base64');
console.log('decoded length:', buf.length, 'byte length:', buf.byteLength);

var base64Buf = buf.slice(0, buf.byteLength);
console.log('base64 buf length:', base64Buf.length);
