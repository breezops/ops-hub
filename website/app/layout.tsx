import type { Metadata } from "next";
import "./globals.css";

export const metadata: Metadata = {
  title: "OPS HUB — Stray Co-op & Game Tools",
  description: "Two cats. One adventure. Together. Download the Stray Co-op launcher, explore the interactive demo, and start a two-player session with a friend.",
  icons: {
    icon: "/favicon.svg",
    shortcut: "/favicon.svg",
  },
};

export default function RootLayout({
  children,
}: Readonly<{
  children: React.ReactNode;
}>) {
  return (
    <html lang="en">
      <body className="antialiased">{children}</body>
    </html>
  );
}
