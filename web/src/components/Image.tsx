import { Box, Props as BoxProps } from "./Box";

export type Props = Omit<BoxProps<"img">, "alt">;

export const Image = ({ ...rest }: Props) => <Box as="img" alt="" {...rest} />;
