function sleep(ms = 0) {
  return new Promise(resolve => setTimeout(resolve, ms));
}

module.exports.handler = (data) => sleep(Math.floor(Math.random() * (750 - 250 + 1)) + 250);
