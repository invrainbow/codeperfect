import { Footer } from "@/components/Footer";
import { Header } from "@/components/Header";
import { Features } from "./Features";
import { Hero } from "./Hero";
import { Intro } from "./Intro";
import dom from "./components/dom";

export const App = () => (
  <dom.div cx="flex flex-col items-stretch min-h-screen font-sans antialiased text-neutral-600">
    <dom.div cx="flex-grow">
      <Header />
      <dom.div cx="flex flex-col mx-auto w-full gap-24 py-12 md:py-20">
        <Hero />
        <Intro />
        <Features />
      </dom.div>
    </dom.div>
    <Footer />
  </dom.div>
);
