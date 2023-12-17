import { Footer } from "@/components/Footer";
import { Header } from "@/components/Header";
import cx from "classnames";
import { Features } from "./Features";
import { Hero } from "./Hero";
import { Intro } from "./Intro";

export const App = () => (
  <div className={cx("flex flex-col min-h-screen font-sans")}>
    <div className="flex-grow">
      <Header />
      <div className="mx-auto w-full flex flex-col gap-24 py-12 md:py-20">
        <Hero />
        <Intro />
        <Features />
      </div>
    </div>
    <Footer />
  </div>
);
