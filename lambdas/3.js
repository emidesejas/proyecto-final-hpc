const fs = require('fs');

function readFileContent(filename) {
  try {
    const data = fs.readFileSync(filename, 'utf8');
    return data;
  } catch (err) {
    console.error(`Error reading file from disk: ${err}`);
  }
}

module.exports.handler = (data) => {
  return { data: readFileContent('lambdas/samplefile') };
};
