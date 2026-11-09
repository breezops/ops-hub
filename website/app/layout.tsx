import type { Metadata } from "next";
import "./globals.css";

export const metadata: Metadata = {
  title: "OPS HUB â€” Stray Co-op & Game Tools",
  description: "Discover independent co-op mods and game launchers. Browse available experiments, explore upcoming projects, and find your next shared adventure.",
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
