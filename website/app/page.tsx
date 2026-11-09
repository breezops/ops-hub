import markup from "./landing.html?raw";
import release from "../public/release.json";
import LandingInteractions from "./landing-interactions";

export default function Home() {
  const html = markup.replaceAll("__RELEASE__", release.version)
    .replaceAll("__PROTOCOL__", String(release.protocol));
  return <><div dangerouslySetInnerHTML={{ __html: html }} /><LandingInteractions /></>;
}
