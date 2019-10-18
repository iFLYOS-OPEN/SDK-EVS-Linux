function system(cmd, cb) {
  return native.system(cmd, cb);
}

function getWifiMac(name) {
  return native.getWifiMac(name);
}

function rand() {
  return native.rand();
}

exports.system = system;
exports.getWifiMac = getWifiMac;
exports.sha = native.sha;
exports.rand = rand;
