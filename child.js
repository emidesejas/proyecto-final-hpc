// child.js
process.stdin.on('data', (data) => {
  let modifiedData = `Modified: ${data}`;
  process.stdout.write(modifiedData);
});
