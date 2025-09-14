export async function handler(event, context) {
  try {
    const mac = event.queryStringParameters?.mac;
    if (!mac) {
      return {
        statusCode: 400,
        body: JSON.stringify({ authorized: false, error: "No MAC provided" }),
      };
    }

    // Fetch whitelist from Firebase Realtime Database
    const firebaseURL = "https://plant-monitor-78d59-default-rtdb.firebaseio.com/whitelist.json";
    const response = await fetch(firebaseURL);
    if (!response.ok) {
      return {
        statusCode: 500,
        body: JSON.stringify({ authorized: false, error: "Failed to fetch whitelist" }),
      };
    }

    const whitelist = await response.json(); // { "MAC1": true, "MAC2": true }
    const authorized = whitelist && mac in whitelist;

    return {
      statusCode: 200,
      body: JSON.stringify({ authorized }),
    };
  } catch (err) {
    return {
      statusCode: 500,
      body: JSON.stringify({ authorized: false, error: err.message }),
    };
  }
}
