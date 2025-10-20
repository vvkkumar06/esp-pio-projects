const express = require('express');
const fetch = require('node-fetch'); // or axios
const app = express();

app.get('/checkWhitelist', async (req, res) => {
  const mac = req.query.mac;
  if (!mac) return res.status(400).json({ authorized: false, error: "No MAC provided" });

  try {
    const firebaseURL = "https://plant-monitor-78d59-default-rtdb.firebaseio.com/whitelist.json";
    const response = await fetch(firebaseURL);
    if (!response.ok) throw new Error("Failed to fetch whitelist");

    const whitelist = await response.json();
    const authorized = whitelist && mac in whitelist;

    res.json({ authorized });
  } catch (err) {
    res.status(500).json({ authorized: false, error: err.message });
  }
});

// Listen on a custom HTTP port
const PORT = process.env.PORT || 10080;
app.listen(PORT, () => console.log(`Server running on port ${PORT}`));
