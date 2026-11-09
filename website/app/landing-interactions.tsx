"use client";
import { useEffect } from "react";
import { initializeHub } from "./hub-interactions";
export default function LandingInteractions() {
  useEffect(initializeHub, []);
  return null;
}
