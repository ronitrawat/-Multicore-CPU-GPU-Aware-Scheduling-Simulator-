import React from "react";
import { createRoot } from "react-dom/client";
import Dashboard from "./ui/Dashboard.jsx";
import "./ui/styles.css";

createRoot(document.getElementById("root")).render(
  <React.StrictMode>
    <Dashboard />
  </React.StrictMode>
);
