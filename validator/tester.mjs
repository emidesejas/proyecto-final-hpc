const API_ENDPOINT = 'http://localhost:8848/lambda/';
const LAMBAS = 1;
const NUMBER_OF_CALLS = 100;

const makeTestCalls = async () => {
  const testCalls = [];

  // Create test calls using fetch
  for (let i = 0; i < NUMBER_OF_CALLS; i++) {
    testCalls.push({
      call: fetch(API_ENDPOINT + 1),
      lambda: i,
      start: Date.now(),
    });
  }

  // Wait for all test calls to complete
  // and log the results
  const results = await Promise.all(testCalls.map(async (testCall) => {
    const response = await testCall.call;
    const end = Date.now();
    const duration = end - testCall.start;

    const body = await response.json();

    console.log({ duration, status: response.status, body });
    return {
      lambda: testCall.lambda,
      duration,
      status: response.status,
    };
  }));
};

await makeTestCalls();

export {};
