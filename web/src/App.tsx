import { Footer } from "@/components/Footer";
import { Header } from "@/components/Header";
import { Features } from "./Features";
import { Hero } from "./Hero";
import { Intro } from "./Intro";
import { Box } from "./components/Box";
import { Flex } from "./components/Flex";

export const App = () => (
  <Flex cx="flex-col items-stretch min-h-screen font-sans antialiased text-neutral-600">
    <Box cx="flex-grow">
      <Header />
      <Flex cx="flex-col mx-auto w-full gap-24 py-12 md:py-20">
        <Hero />
        <Intro />
        <Features />
      </Flex>
    </Box>
    <Footer />
  </Flex>
);
