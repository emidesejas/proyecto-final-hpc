const https = require('https');

module.exports.handler = (data) => {
  return new Promise((resolve, reject) => {
    // make a request to google.com
    https.get('https://cat-fact.herokuapp.com/facts', (res) => {
      let responseData = '';
      // collect the response data
      res.on('data', (chunk) => {
        responseData += chunk;
      });
      // return the response data
      res.on('end', () => {
        resolve(responseData);
      });
    }).on('error', (err) => {
      console.error(err);
      reject(err);
    });
  });
};
