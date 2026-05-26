import { initializeApp } from "https://www.gstatic.com/firebasejs/10.14.1/firebase-app.js";
import { getAuth } from "https://www.gstatic.com/firebasejs/10.14.1/firebase-auth.js";
import { getDatabase } from "https://www.gstatic.com/firebasejs/10.14.1/firebase-database.js";

export const firebaseConfig = {
  apiKey: "AIzaSyAHeN_fylzenZiANfd_RphzsqBuxjif3sY",
  authDomain: "temp-hum-c4be4.firebaseapp.com",
  databaseURL: "https://temp-hum-c4be4-default-rtdb.europe-west1.firebasedatabase.app",
  projectId: "temp-hum-c4be4",
  storageBucket: "temp-hum-c4be4.firebasestorage.app",
  messagingSenderId: "788419887651",
  appId: "1:788419887651:web:1713c9f2595c539c04a796"
};

export const app = initializeApp(firebaseConfig);
export const auth = getAuth(app);
export const db = getDatabase(app);