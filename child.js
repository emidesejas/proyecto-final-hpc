// child.js
const fs = require('fs');

const fifos = process.argv.slice(2);
console.info(`Node.JS child process ${process.pid} started with args: ${fifos.join(', ')}`);

const [toNodeFifo, fromNodeFifo] = fifos;

let outFileDescriptor;

const returnToParent = (requestNumber, result) => {
  const writeStream = fs.createWriteStream(null, { fd: outFileDescriptor });

  writeStream.on('error', (err) => {
    console.error(err);
  });

  if (result === undefined || result === null) {
    result = 'null';
  }

  const dataString = result instanceof Object ? JSON.stringify(result) : result.toString();
  const dataSize = Buffer.alloc(8); 
  dataSize.writeBigInt64LE(BigInt(dataString.length));

  // Has size and data in the same buffer
  const combinedBuffer = Buffer.concat([dataSize, Buffer.from(dataString)]);

  console.log(`NodeJS will write for request number: ${requestNumber}:`, dataSize.toString(), combinedBuffer.toString());

  writeStream.write(combinedBuffer, () => {
    writeStream.end();
  });
}

fs.open(toNodeFifo, fs.constants.O_RDONLY, (err, inFileDescriptor) => {
  if (err) throw err;
  const readStream = fs.createReadStream(null, { fd: inFileDescriptor });
  

  readStream.on('data', (data) => {
    // data is received from parent process

    fs.open(fromNodeFifo, fs.constants.O_WRONLY, (err, fd) => {
      if (err) throw err;
      outFileDescriptor = fd;
    });

    const lambdaId = data.toString().split(',')[0];
    const requestNumber = data.toString().split(',')[1];

    console.info(`Node.JS lambda id: ${lambdaId} for request number: ${requestNumber}`);
    console.info(`Node.JS out descriptor: ${outFileDescriptor} for request number: ${requestNumber}`);
    
    const { handler } = require(`./lambdas/${lambdaId}.js`);
    const result = handler(requestNumber);
    if (result instanceof Promise) {
      result.then((result) => {
        console.log(`NodeJS will return for request number: ${requestNumber}:`, result);
        returnToParent(requestNumber, result);
      });
    } else {
      console.log(`NodeJS will return for request number: ${requestNumber}:`, result);
      returnToParent(requestNumber, result);
    }
  });
});
