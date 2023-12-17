import { Features } from "./Features";
import { Hero } from "./Hero";
import { Intro } from "./Intro";

export const Home = () => (
  <div className="mx-auto w-full flex flex-col gap-24 py-12 md:py-20">
    <Hero />
    <Intro />
    <Features />
  </div>
);
