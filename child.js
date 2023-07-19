// child.js
const fs = require('fs');

const fifos = process.argv.slice(2);
console.info(`Node.JS child process ${process.pid} started with args: ${fifos.join(', ')}`);

const [toNodeFifo, fromNodeFifo] = fifos;

fs.open(toNodeFifo, fs.constants.O_RDONLY, (err, inFileDescriptor) => {
  if (err) throw err;
  const readStream = fs.createReadStream(null, { fd: inFileDescriptor });
  

  readStream.on('data', (data) => {
    // data is received from parent process

    console.info('Node.JS booting lambda: ', data.toString());
    const { handler } = require(`./lambdas/${data}.js`)
    const result = handler(data.toString());

    fs.open(fromNodeFifo, fs.constants.O_WRONLY, (err, outFileDescriptor) => {
      if (err) throw err;

      const writeStream = fs.createWriteStream(null, { fd: outFileDescriptor });
      writeStream.write(JSON.stringify(result), () => {
        writeStream.end();
      });
    });
  });

});
